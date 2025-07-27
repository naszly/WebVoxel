#include "ChunkManagementSystem.h"

void ChunkManagementSystem::initialize() {
    for (auto& worker : m_chunkWorkers) {
        worker = std::make_unique<Threading::Worker>();
        worker->start(ChunkManagementSystem::worker, this);
    }
}

void ChunkManagementSystem::update(float dt) {
    const Camera& camera = getCamera();
    World& world = getWorld();

    loadChunks(camera, world);

    updateSaveQueue(world);

    unloadChunks(camera, world);
}

void ChunkManagementSystem::loadChunks(const Camera &camera, World &world) {
    Threading::ScopedLock lock(&m_lock);

    for (auto& chunk : m_loadedChunks) {
        world.moveChunk(chunk);
    }
    m_loadedChunks.clear();

    updateLoadQueue(camera, world);
}

std::vector<glm::ivec3> ChunkManagementSystem::generateChunkOffsets() {
    std::vector<glm::ivec3> offsets;
    for (int x = -LOAD_RADIUS; x <= LOAD_RADIUS; ++x) {
        for (int y = -LOAD_RADIUS; y <= LOAD_RADIUS; ++y) {
            for (int z = -LOAD_RADIUS; z <= LOAD_RADIUS; ++z) {
                if (x*x + y*y + z*z <= LOAD_RADIUS*LOAD_RADIUS) {
                    offsets.emplace_back(x, y, z);
                }
            }
        }
    }
    std::ranges::sort(offsets, [](const glm::ivec3& a, const glm::ivec3& b) {
        return a.x*a.x + a.y*a.y + a.z*a.z < b.x*b.x + b.y*b.y + b.z*b.z;
    });
    return offsets;
}

void ChunkManagementSystem::updateLoadQueue(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    static std::vector<glm::ivec3> offsets = generateChunkOffsets();

    std::vector<glm::ivec3> chunksToLoad;
    const size_t maxChunksToLoad = m_chunkWorkersCount * 3;
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

void ChunkManagementSystem::updateSaveQueue(World& world) {
    const auto chunks = world.getChunks();

    for (auto &chunk : chunks) {
        if (chunk.isSaveFileDirty() && !m_savingChunks.contains(chunk.getPosition())) {
            Threading::ScopedLock lock(&m_lock);
            m_chunksToSave.push(chunk);
            m_savingChunks.insert(chunk.getPosition());
            chunk.resetSaveFileDirty();
        }
    }
}

void ChunkManagementSystem::unloadChunks(const Camera &camera, World &world) {
    const glm::vec3 playerPosition = camera.getPosition();

    const auto chunks = world.getChunks();

    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunk : chunks) {
        auto chunkPos = chunk.getPosition();
        if (getChunkDistance(playerPosition, chunkPos) > UNLOAD_RADIUS) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    for (const auto &chunkPos : chunksToUnload) {
        world.removeChunk(chunkPos);
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    const auto transformedPlayerPosition = playerPosition * glm::vec3(1.0f, 1.33f, 1.0f);
    const auto transformedChunkPos = glm::vec3(chunkPos) * glm::vec3(1.0f, 1.33f, 1.0f);
    return glm::distance(glm::vec3(transformedChunkPos), glm::vec3(WorldCoordinate(transformedPlayerPosition).chunkPosition()));
}

void* ChunkManagementSystem::worker(void *arg) {
    const auto system = static_cast<ChunkManagementSystem*>(arg);
    std::optional<Chunk> chunkToSave = std::nullopt;
    std::optional<glm::ivec3> chunkToLoad = std::nullopt;
    bool shouldExit = false;
    while (!shouldExit) {
        {
            Threading::ScopedLock lock(&system->m_lock);

            if (system->m_shouldExit && system->m_chunksToSave.empty()) {
                shouldExit = true;
                continue;
            }

            if (!system->m_chunksToSave.empty()) {
                chunkToSave = std::move(system->m_chunksToSave.front());
                system->m_chunksToSave.pop();
                system->m_savingChunks.emplace(chunkToSave.value().getPosition());
            } else if (!system->m_chunksToLoad.empty()) {
                chunkToLoad = system->m_chunksToLoad.front();
                system->m_chunksToLoad.pop();
                system->m_loadingChunks.emplace(chunkToLoad.value());
            }
        }

        if (chunkToSave.has_value()) {
            auto& chunk = chunkToSave.value();
            auto chunkPos = chunk.getPosition();

            chunk.save();
            chunkToSave = std::nullopt;

            Threading::ScopedLock lock(&system->m_lock);

            system->m_savingChunks.erase(chunkPos);
        } else if (chunkToLoad.has_value()) {
            const auto chunkPos = chunkToLoad.value();
            Chunk chunk(chunkPos);

            if (chunk.fileExists()) {
                chunk.load();
            } else {
                chunk.generate(system->m_fnGenerator);
            }

            chunkToLoad = std::nullopt;

            Threading::ScopedLock lock(&system->m_lock);

            system->m_loadedChunks.emplace_back(std::move(chunk));
            system->m_loadingChunks.erase(chunkPos);
        } else {
            Threading::sleep(200);
        }
    }

    return nullptr;
}
