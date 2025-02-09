#include "ChunkManagementSystem.h"

void ChunkManagementSystem::initialize() {
    for (auto& worker : m_LoadChunksWorkers) {
        worker = std::make_unique<Threading::Worker>();
        worker->start(ChunkManagementSystem::worker, this);
    }
}

void ChunkManagementSystem::update(float dt) {
    const Camera& camera = GetCamera();
    World& world = GetWorld();

    loadChunks(camera, world);

    unloadChunks(camera, world);
}

void ChunkManagementSystem::loadChunks(const Camera &camera, World &world) {
    Threading::ScopedLock lock(&m_Lock);

    for (auto& chunk : m_LoadedChunks) {
        world.moveChunk(chunk);
    }
    m_LoadedChunks.clear();

    updateLoadQueue(camera, world);
}

void ChunkManagementSystem::updateLoadQueue(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    std::vector<glm::ivec3> chunksToLoad;
    for (int x = -m_LoadRadius; x <= m_LoadRadius; x++) {
        for (int y = -m_LoadRadius; y <= m_LoadRadius; y++) {
            for (int z = -m_LoadRadius; z <= m_LoadRadius; z++) {
                const auto chunkPos = playerChunk + glm::ivec3(x, y, z);
                if (getChunkDistance(playerPosition, chunkPos) <= m_LoadRadius && !world.hasChunk(chunkPos)) {
                    chunksToLoad.push_back(chunkPos);
                }
            }
        }
    }

    std::ranges::sort(chunksToLoad, [&](const glm::ivec3& a, const glm::ivec3& b) {
        return getChunkDistance(playerPosition, a) < getChunkDistance(playerPosition, b);
    });

    m_LoadQueue = chunksToLoad;
}

void ChunkManagementSystem::unloadChunks(const Camera &camera, World &world) {
    const glm::vec3 playerPosition = camera.getPosition();

    const auto chunks = world.getChunks();

    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunk : chunks) {
        auto chunkPos = chunk.getPosition();
        if (getChunkDistance(playerPosition, chunkPos) > m_UnloadRadius) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    for (const auto &chunkPos : chunksToUnload) {
        world.removeChunk(chunkPos);
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    return glm::distance(glm::vec3(chunkPos), glm::vec3(WorldCoordinate(playerPosition).chunkPosition()));
}

void* ChunkManagementSystem::worker(void *arg) {
    const auto system = static_cast<ChunkManagementSystem*>(arg);
    std::optional<glm::ivec3> chunkToLoad = std::nullopt;
    while (!system->m_ShouldExit) {
        {
            Threading::ScopedLock lock(&system->m_Lock);
            if (!system->m_LoadQueue.empty()) {
                chunkToLoad = system->m_LoadQueue.front();
            }
        }

        if (!chunkToLoad.has_value()) {
            Threading::Sleep(200);
        } else {
            const auto chunkPos = chunkToLoad.value();
            Chunk chunk(chunkPos);

            const bool existingChunk = chunk.fileExists();

            if (existingChunk) {
                chunk.load();
            } else {
                chunk.generate(system->fnGenerator);
                chunk.save();
            }

            chunkToLoad = std::nullopt;

            Threading::ScopedLock lock(&system->m_Lock);

            system->m_LoadedChunks.emplace_back(std::move(chunk));
            if (auto it = std::ranges::find(system->m_LoadQueue, chunkPos); it != system->m_LoadQueue.end()) {
                system->m_LoadQueue.erase(it);
            }
        }
    }

    return nullptr;
}
