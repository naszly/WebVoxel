#include "ChunkManagementSystem.h"

#include "RendererSystem.h"
#include "application/Application.h"
#include "common/Log.h"

void ChunkManagementSystem::initialize() {

    const auto hardwareConcurrency = Threading::hardwareConcurrency();

    m_chunkWorkersCount = 1;

    if (hardwareConcurrency > 2) {
        m_chunkWorkersCount = m_chunkWorkersCount = std::min(hardwareConcurrency / 2, 5u);
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

    integrateLoadedChunks(world);
    integrateCompressedChunks(world);

    static int turn = 0;
    turn = (turn + 1) % 4;
    if (turn == 0) {
        scheduleChunksForLoading(camera, world);
    } else if (turn == 1) {
        scheduleChunksForSaving(world);
    } else if (turn == 2) {
        scheduleChunksForUnloading(camera, world);
    } else if (turn == 3) {
        scheduleChunksForCompression(world, camera);
    }
}

void ChunkManagementSystem::integrateLoadedChunks(World &world) {
    for (auto& chunk : m_loadedChunks) {
        world.insertChunkByMove(*chunk);
        m_loadingChunks.erase(chunk->getPosition());
    }
    m_loadedChunks.clear();
}

void ChunkManagementSystem::integrateCompressedChunks(World& world) {
    for (auto& task : m_compressedChunks) {
        if (auto* chunk = world.tryGetChunkPtr(task.position)) {
            if (chunk->getLastEdit() == task.lastAccess) {
                *chunk = std::move(*task.chunk);
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

void ChunkManagementSystem::scheduleChunksForLoading(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    static std::vector<glm::ivec3> offsets = generateChunkOffsets();

    std::vector<glm::ivec3> chunksToLoad;
    const size_t maxChunksToLoad = m_chunkWorkersCount * 32;

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
    for (auto &chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
        if (chunk.isSaveFileDirty()) {
            if (!m_savingChunks.contains(chunk.getPosition())) {
                m_chunksToSave.push(ChunkHandle::makeCopy(chunk));
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

    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
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
        m_chunksToUnload.push(ChunkHandle(world.extractChunkByMove(chunkPos)));
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
    for (auto& chunkRef : world.getChunks()) {
        auto& chunk = chunkRef.get();
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
        CompressionTask task{chunkPos, ChunkHandle::makeCopy(*chunk), chunk->getLastEdit()};
        m_chunksToCompress.push(std::move(task));
        m_compressingChunks.insert(chunkPos);
        ++count;
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    const auto transformedPlayerPosition = playerPosition * glm::vec3(1.0f, 1.0f, 1.0f);
    const auto transformedChunkPos = glm::vec3(chunkPos) * glm::vec3(1.0f, 1.0f, 1.0f);
    return glm::distance(glm::vec3(transformedChunkPos), glm::vec3(WorldCoordinate(transformedPlayerPosition).chunkPosition()));
}

void ChunkManagementSystem::handleChunkSave(ChunkHandle& chunkToSave) {
    const auto chunkPos = chunkToSave->getPosition();
    chunkToSave->save();

    Threading::ScopedLock lock(&m_lock);
    m_savingChunks.erase(chunkPos);
}

void ChunkManagementSystem::handleChunkLoad(const glm::ivec3& chunkToLoad) {
    Chunk chunk(chunkToLoad);
    if (chunk.fileExists()) {
        chunk.load();
    } else {
        chunk.generate(m_generator);
    }

    Threading::ScopedLock lock(&m_lock);
    m_loadedChunks.emplace_back(std::move(chunk));
}

void ChunkManagementSystem::handleChunkCompression(CompressionTask& task) {
    task.chunk->compress();
    Threading::ScopedLock lock(&m_lock);
    m_compressedChunks.push_back(std::move(task));
}

void* ChunkManagementSystem::worker(void *arg) {
    auto& system = *static_cast<ChunkManagementSystem*>(arg);

    Work work;

    while (system.fetchWork(work)) {

        if (!work.hasPendingWork()) {
            Threading::sleep(20);
            continue;
        }

        while (!work.chunksToUnload.empty()) {
            work.chunksToUnload.pop();
        }

        if (work.chunkToSave.has_value()) {
            system.handleChunkSave(*work.chunkToSave);
            work.chunkToSave.reset();
        }

        if (work.chunkToLoad.has_value()) {
            system.handleChunkLoad(*work.chunkToLoad);
            work.chunkToLoad.reset();
        }

        if (work.compressionTask.has_value()) {
            system.handleChunkCompression(*work.compressionTask);
            work.compressionTask.reset();
        }

        Threading::sleep(1);
    }

    return nullptr;
}

bool ChunkManagementSystem::fetchWork(Work& work) {
    Threading::ScopedLock lock(&m_lock);

    if (m_shouldExit && m_chunksToSave.empty()) {
        return false;
    }

    if (!m_chunksToSave.empty()) {
        work.chunkToSave = std::move(m_chunksToSave.front());
        m_chunksToSave.pop();
        m_savingChunks.emplace(work.chunkToSave.value()->getPosition());
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
