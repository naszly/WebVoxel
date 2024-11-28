#include "ChunkManagementSystem.h"

void ChunkManagementSystem::initialize() {
    m_LoadThread = std::thread([this] {
        std::optional<glm::ivec3> chunkToLoad = std::nullopt;
        while (!m_ShouldExit) {
            {
                std::lock_guard lock(m_Mutex);
                if (!m_LoadQueue.empty()) {
                    chunkToLoad = m_LoadQueue.front();
                }
            }

            if (!chunkToLoad.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                const auto chunkPos = chunkToLoad.value();
                Chunk chunk(chunkPos);
                if (chunkPos.y < 0) {
                    chunk.generate();
                }

                chunkToLoad = std::nullopt;

                std::lock_guard lock(m_Mutex);
                m_LoadedChunks.push_back(chunk);
                if (auto it = std::ranges::find(m_LoadQueue, chunkPos); it != m_LoadQueue.end()) {
                    m_LoadQueue.erase(it);
                }
            }
        }
    });
}

void ChunkManagementSystem::update(float dt) {
    const Camera& camera = GetCamera();
    World& world = GetWorld();

    loadChunks(camera, world);

    unloadChunks(camera, world);
}

void ChunkManagementSystem::loadChunks(const Camera &camera, World &world) {
    std::lock_guard lock(m_Mutex);

    for (auto& chunk : m_LoadedChunks) {
        world.setChunk(chunk);
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
