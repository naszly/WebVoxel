#include "ChunkManagementSystem.h"

#include "RendererSystem.h"
#include "application/Application.h"
#include "application/meshing/VoxelVertexGenerator.h"
#include "common/Log.h"

void ChunkManagementSystem::initialize() {

    const auto hardwareConcurrency = Threading::hardwareConcurrency();

    m_chunkWorkersCount = 1;

    if (hardwareConcurrency > 2) {
        m_chunkWorkersCount = m_chunkWorkersCount = std::min(hardwareConcurrency / 2, 6u);
    }

    for (size_t i = 0; i < m_chunkWorkersCount; ++i) {
        m_chunkWorkers.push_back(std::make_unique<Threading::Worker>());
        m_chunkWorkers[i]->start(worker, this);
    }
}

void ChunkManagementSystem::update(const float dt) {
    const Camera& camera = getCamera();
    World& world = getWorld();

    processChunkManagement(camera, world);

    m_generator.pruneCacheByDistance(camera.getPosition(), UNLOAD_ZONE_RADIUS_XZ);
}

void ChunkManagementSystem::processChunkManagement(const Camera& camera, World& world) {
    Threading::ScopedLock lock(&m_lock);

    integrateCreatedChunkVertexData();
    integrateLoadedChunks(world);
    integrateCompressedChunks(world);

    scheduleChunksForVertexDataCreation(camera, world);
    scheduleChunksForLoading(camera, world);
    scheduleChunksForSaving(world);
    scheduleChunksForUnloading(camera, world);
    scheduleChunksForCompression(world, camera);
}

void ChunkManagementSystem::integrateCreatedChunkVertexData() {
    const auto rendererSystem = getApplication().getSystem<RendererSystem>();

    for (const auto& [chunkPos, vertexData] : m_dirtyChunkVertexDatas) {
        rendererSystem->updateChunkVertexBuffer(vertexData, chunkPos);
    }

    m_dirtyChunkVertexDatas.clear();
}

void ChunkManagementSystem::integrateLoadedChunks(World &world) {
    for (auto& chunk : m_loadedChunks) {
        world.insertChunkByMove(chunk);
        m_loadingChunks.erase(chunk.getPosition());
    }
    m_loadedChunks.clear();
}

void ChunkManagementSystem::integrateCompressedChunks(World& world) {
    for (auto& task : m_compressedChunks) {
        if (auto* chunk = world.tryGetChunk(task.position)) {
            if (chunk->getLastEdit() == task.lastAccess) {
                *chunk = std::move(task.chunk);
            } else {
                LogApp::warning("Chunk at ({}, {}, {}) was modified after compression, skipping integration",
                                task.position.x, task.position.y, task.position.z);
            }
        } else {
            LogApp::warning("Chunk at ({}, {}, {}) not found for integration after compression",
                            task.position.x, task.position.y, task.position.z);
        }
        m_compressingChunks.erase(task.position);
    }
    m_compressedChunks.clear();
}

std::vector<glm::ivec3> ChunkManagementSystem::generateChunkOffsets() {
    std::vector<glm::ivec3> offsets;
    constexpr float yCorrection = static_cast<float>(LOAD_ZONE_RADIUS_XZ) / LOAD_ZONE_RADIUS_Y;
    for (int x = -LOAD_ZONE_RADIUS_XZ; x <= LOAD_ZONE_RADIUS_XZ; ++x) {
        for (int y = -LOAD_ZONE_RADIUS_Y; y <= LOAD_ZONE_RADIUS_Y; ++y) {
            const auto cy = static_cast<float>(y) * yCorrection;
            for (int z = -LOAD_ZONE_RADIUS_XZ; z <= LOAD_ZONE_RADIUS_XZ; ++z) {
                if (x * x + z * z + cy * cy <= LOAD_ZONE_RADIUS_XZ * LOAD_ZONE_RADIUS_XZ) {
                    offsets.emplace_back(x, y, z);
                }
            }
        }
    }
    std::ranges::sort(offsets, [](const glm::ivec3& a, const glm::ivec3& b) {
        const float cay = static_cast<float>(a.y) * yCorrection;
        const float cby = static_cast<float>(b.y) * yCorrection;
        const float da = a.x * a.x + a.z * a.z + cay * cay;
        const float db = b.x * b.x + b.z * b.z + cby * cby;
        return da < db;
    });
    return offsets;
}

void ChunkManagementSystem::scheduleChunksForVertexDataCreation(const Camera& camera, World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();
    const auto& chunks = world.getChunks();
    std::vector<std::reference_wrapper<Chunk>> dirty;

    std::ranges::copy_if(chunks, std::back_inserter(dirty), [&](const Chunk &chunk) {
        if (!chunk.isGpuBufferDirty()) {
            return false;
        }
        const auto neighborhood = world.getChunkNeighborhoodPtrs(chunk.getPosition());
        return neighborhood.hasAllNeighbours();
    });

    std::ranges::sort(dirty, [&](const Chunk &a, const Chunk &b) {
        const auto aPos = a.getPosition();
        const auto bPos = b.getPosition();
        return Utils::distance2(aPos, playerChunk) < Utils::distance2(bPos, playerChunk);
    });

    // free old ChunkNeighborhoods on the same thread they were allocated
    while (!m_freeChunkNeighborhoods.empty()) {
        m_freeChunkNeighborhoods.pop();
    }

    for (auto& chunkRef : dirty) {
        auto& chunk = chunkRef.get();
        auto position = chunk.getPosition();
        auto chunkNeighborhood = world.getChunkNeighborhood(position);
        if (!chunkNeighborhood.hasAllNeighbours()) {
            continue;
        }
        const bool success = m_dirtyChunks.push(std::move(chunkNeighborhood));
        if (!success) {
            break;
        }
        chunk.resetGpuBufferDirty();
    }
}

void ChunkManagementSystem::scheduleChunksForLoading(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    static std::vector<glm::ivec3> offsets = generateChunkOffsets();

    std::vector<glm::ivec3> chunksToLoad;
    const size_t maxChunksToLoad = m_chunkWorkersCount * 8;

    for (const auto& offset : offsets) {
        const auto chunkPos = playerChunk + offset;

        if (world.hasChunk(chunkPos) || m_loadingChunks.contains(chunkPos)) {
            continue;
        }

        chunksToLoad.push_back(chunkPos);

        if (chunksToLoad.size() >= maxChunksToLoad) {
            break;
        }
    }

    m_chunksToLoad = std::queue(chunksToLoad.begin(), chunksToLoad.end());
}

void ChunkManagementSystem::scheduleChunksForSaving(World& world) {
    const auto chunks = world.getChunks();

    for (auto &chunk : chunks) {
        if (chunk.isSaveFileDirty()) {
            if (!m_savingChunks.contains(chunk.getPosition())) {
                m_chunksToSave.push(chunk);
                m_savingChunks.insert(chunk.getPosition());
                chunk.resetSaveFileDirty();
            }
        }
    }
}

void ChunkManagementSystem::scheduleChunksForUnloading(const Camera &camera, World &world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();
    constexpr float yCorrection = static_cast<float>(UNLOAD_ZONE_RADIUS_XZ) / UNLOAD_ZONE_RADIUS_Y;

    const auto chunks = world.getChunks();
    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunk : chunks) {
        auto chunkPos = chunk.getPosition();
        const glm::ivec3 offset = chunkPos - playerChunk;
        const float correctedOffestY = static_cast<float>(offset.y) * yCorrection;
        const float distanceSq = offset.x * offset.x + offset.z * offset.z + correctedOffestY * correctedOffestY;
        constexpr float unloadRadiusSq = static_cast<float>(UNLOAD_ZONE_RADIUS_XZ * UNLOAD_ZONE_RADIUS_XZ);
        if (distanceSq > unloadRadiusSq) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    for (const auto &chunkPos : chunksToUnload) {
        m_chunksToUnload.push(world.extractChunkByMove(chunkPos));
    }
}

void ChunkManagementSystem::scheduleChunksForCompression(World& world, const Camera& camera) {
    const glm::vec3 playerPosition = camera.getPosition();

    constexpr size_t maxChunksToSchedule = 2;

    if (m_compressingChunks.size() >= maxChunksToSchedule) {
        return;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::vector<Chunk*> candidates;
    for (auto& chunk : world.getChunks()) {
        const glm::ivec3 chunkPos = chunk.getPosition();
        if (chunk.isCompressed()) continue;
        if (chunk.isGpuBufferDirty() || chunk.isSaveFileDirty()) continue;
        if (chunk.getLastEdit() - now < std::chrono::seconds(15)) continue;
        if (getChunkDistance(playerPosition, chunkPos) <= FAST_ACCESS_RADIUS) continue;
        if (m_compressingChunks.contains(chunkPos)) continue;

        const auto& chunkNeighborhood = world.getChunkNeighborhoodPtrs(chunkPos);
        if (!chunkNeighborhood.hasAllNeighbours()) continue;
        if (chunkNeighborhood.anyNeighbourDirty()) continue;

        candidates.push_back(&chunk);
    }

    std::ranges::sort(candidates, [](const Chunk* a, const Chunk* b) {
        return a->getLastEdit() < b->getLastEdit();
    });

    size_t count = 0;
    for (const auto* chunk : candidates) {
        if (count >= maxChunksToSchedule) break;
        const glm::ivec3 chunkPos = chunk->getPosition();
        CompressionTask task{chunkPos, *chunk, chunk->getLastEdit()};
        m_chunksToCompress.push(task);
        m_compressingChunks.insert(chunkPos);
        ++count;
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    const auto transformedPlayerPosition = playerPosition * glm::vec3(1.0f, 1.0f, 1.0f);
    const auto transformedChunkPos = glm::vec3(chunkPos) * glm::vec3(1.0f, 1.0f, 1.0f);
    return glm::distance(glm::vec3(transformedChunkPos), glm::vec3(WorldCoordinate(transformedPlayerPosition).chunkPosition()));
}

void ChunkManagementSystem::handleChunkVertexDataCreation(ChunkNeighborhood& chunkNeighborhood) {
    const std::optional<Chunk>& chunk = chunkNeighborhood.getCenterChunk();

    auto position = chunk->getPosition();
    if (!chunkNeighborhood.hasAllNeighbours()) {
        return;
    }

    thread_local std::vector<VertexData> points;
    points.clear();
    VoxelVertexGenerator::generate(chunkNeighborhood, points);

    Threading::ScopedLock lock(&m_lock);
    m_dirtyChunkVertexDatas.emplace(position, std::move(points));
    m_freeChunkNeighborhoods.push(std::move(chunkNeighborhood));
}

void ChunkManagementSystem::handleChunkSave(std::optional<Chunk>& chunkToSave) {
    if (!chunkToSave.has_value()) return;

    auto& chunk = chunkToSave.value();
    const auto chunkPos = chunk.getPosition();
    chunk.save();
    chunkToSave = std::nullopt;

    Threading::ScopedLock lock(&m_lock);
    m_savingChunks.erase(chunkPos);
}

void ChunkManagementSystem::handleChunkLoad(std::optional<glm::ivec3>& chunkToLoad) {
    if (!chunkToLoad.has_value()) return;

    const auto chunkPos = chunkToLoad.value();
    Chunk chunk(chunkPos);
    if (chunk.fileExists()) {
        chunk.load();
    } else {
        chunk.generate(m_generator);
    }
    chunkToLoad = std::nullopt;

    Threading::ScopedLock lock(&m_lock);
    m_loadedChunks.emplace_back(std::move(chunk));
}

void ChunkManagementSystem::handleChunkCompression(std::optional<CompressionTask>& task) {
    if (!task.has_value()) return;

    auto& t = task.value();
    t.chunk.compress();
    Threading::ScopedLock lock(&m_lock);
    m_compressedChunks.push_back(std::move(t));
    task.reset();
}

void* ChunkManagementSystem::worker(void *arg) {
    auto& system = *static_cast<ChunkManagementSystem*>(arg);

    Work work;

    while (system.fetchWork(work)) {

        if (!work.hasPendingWork()) {
            Threading::sleep(20);
            continue;
        }

        if (work.chunkVertexDataToCreate.has_value()) {
            system.handleChunkVertexDataCreation(*work.chunkVertexDataToCreate);
            work.chunkVertexDataToCreate.reset();
        }

        while (!work.chunksToUnload.empty()) {
            work.chunksToUnload.pop();
        }

        if (work.chunkToSave.has_value()) {
            system.handleChunkSave(work.chunkToSave);
        }

        if (work.chunkToLoad.has_value()) {
            system.handleChunkLoad(work.chunkToLoad);
        }

        if (work.compressionTask.has_value()) {
            system.handleChunkCompression(work.compressionTask);
        }
    }

    return nullptr;
}

bool ChunkManagementSystem::fetchWork(Work& work) {
    Threading::ScopedLock lock(&m_lock);

    if (m_shouldExit && m_chunksToSave.empty()) {
        return false;
    }

    if (!m_dirtyChunks.empty()) {
        work.chunkVertexDataToCreate = std::move(m_dirtyChunks.first());
        m_dirtyChunks.removeFirst();
        return true;
    }

    if (!m_chunksToSave.empty()) {
        work.chunkToSave = std::move(m_chunksToSave.front());
        m_chunksToSave.pop();
        m_savingChunks.emplace(work.chunkToSave.value().getPosition());
    }

    if (!m_chunksToLoad.empty()) {
        work.chunkToLoad = m_chunksToLoad.front();
        m_chunksToLoad.pop();
        m_loadingChunks.emplace(work.chunkToLoad.value());
    }

    while (!m_chunksToUnload.empty()) {
        auto& chunk = m_chunksToUnload.front();
        work.chunksToUnload.push(std::move(chunk));
        m_chunksToUnload.pop();
    }

    if (!m_chunksToCompress.empty()) {
        work.compressionTask = std::move(m_chunksToCompress.front());
        m_chunksToCompress.pop();
        m_compressingChunks.emplace(work.compressionTask->position);
    }

    return true;
}
