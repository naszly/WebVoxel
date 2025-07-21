#include "ChunkManagementSystem.h"

void ChunkManagementSystem::initialize() {
    for (auto& worker : m_loadChunksWorkers) {
        worker = std::make_unique<Threading::Worker>();
        worker->start(ChunkManagementSystem::worker, this);
    }
}

void ChunkManagementSystem::update(float dt) {
    const Camera& camera = getCamera();
    World& world = getWorld();

    if (!m_saveInProgress) {
        loadChunks(camera, world);

        unloadChunks(camera, world);
    }
}

void ChunkManagementSystem::saveAllChunks() {
    Threading::ScopedLock lock(&m_lock);

    if (m_saveInProgress)
        return;

    m_saveInProgress = true;

    m_saveChunksWorker = std::make_unique<Threading::Worker>();

    m_saveChunksWorker->start(saveAllChunksWorker, this);
}

void ChunkManagementSystem::loadChunks(const Camera &camera, World &world) {
    Threading::ScopedLock lock(&m_lock);

    for (auto& chunk : m_loadedChunks) {
        world.moveChunk(chunk);
    }
    m_loadedChunks.clear();

    updateLoadQueue(camera, world);
}

void ChunkManagementSystem::updateLoadQueue(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    std::vector<glm::ivec3> chunksToLoad;
    for (int x = -LOAD_RADIUS; x <= LOAD_RADIUS; x++) {
        for (int y = -LOAD_RADIUS; y <= LOAD_RADIUS; y++) {
            for (int z = -LOAD_RADIUS; z <= LOAD_RADIUS; z++) {
                const auto chunkPos = playerChunk + glm::ivec3(x, y, z);
                if (getChunkDistance(playerPosition, chunkPos) <= LOAD_RADIUS && !world.hasChunk(chunkPos)) {
                    chunksToLoad.push_back(chunkPos);
                }
            }
        }
    }

    std::ranges::sort(chunksToLoad, [&](const glm::ivec3& a, const glm::ivec3& b) {
        return getChunkDistance(playerPosition, a) < getChunkDistance(playerPosition, b);
    });

    m_loadQueue = chunksToLoad;
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
    std::optional<glm::ivec3> chunkToLoad = std::nullopt;
    while (!system->m_shouldExit) {
        {
            Threading::ScopedLock lock(&system->m_lock);
            if (!system->m_loadQueue.empty()) {
                chunkToLoad = system->m_loadQueue.front();
            }
        }

        if (!chunkToLoad.has_value()) {
            Threading::sleep(200);
        } else {
            const auto chunkPos = chunkToLoad.value();
            Chunk chunk(chunkPos);

            const bool existingChunk = chunk.fileExists();

            if (existingChunk) {
                chunk.load();
            } else {
                chunk.generate(system->m_fnGenerator);
                chunk.save();
            }

            chunkToLoad = std::nullopt;

            Threading::ScopedLock lock(&system->m_lock);

            system->m_loadedChunks.emplace_back(std::move(chunk));
            if (auto it = std::ranges::find(system->m_loadQueue, chunkPos); it != system->m_loadQueue.end()) {
                system->m_loadQueue.erase(it);
            }
        }
    }

    return nullptr;
}

void* ChunkManagementSystem::saveAllChunksWorker(void *arg) {
    const auto system = static_cast<ChunkManagementSystem*>(arg);

    World& world = getWorld();
    std::vector<glm::ivec3 > chunksToSave;

    {
        Threading::ScopedLock lock(&system->m_lock);

        for (auto& chunk : world.getChunks()) {
            chunksToSave.push_back(chunk.getPosition());
        }
    }

    // this might give a segfault in rare cases...
    for (auto& chunkPos : chunksToSave) {
        if (Chunk* chunk = world.tryGetChunk(chunkPos)) {
            chunk->save();
        }
    }

    system->m_saveInProgress = false;

    return nullptr;
}
