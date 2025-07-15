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

    if (!m_SaveInProgress) {
        loadChunks(camera, world);

        unloadChunks(camera, world);
    }
}

void ChunkManagementSystem::saveAllChunks() {
    Threading::ScopedLock lock(&m_Lock);

    if (m_SaveInProgress)
        return;

    m_SaveInProgress = true;

    m_SaveChunksWorker = std::make_unique<Threading::Worker>();

    m_SaveChunksWorker->start(saveAllChunksWorker, this);
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

    m_LoadQueue = chunksToLoad;
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
                chunk.generate(system->m_fnGenerator);
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

void* ChunkManagementSystem::saveAllChunksWorker(void *arg) {
    const auto system = static_cast<ChunkManagementSystem*>(arg);

    World& world = GetWorld();
    std::vector<glm::ivec3 > chunksToSave;

    {
        Threading::ScopedLock lock(&system->m_Lock);

        for (auto& chunk : world.getChunks()) {
            chunksToSave.push_back(chunk.getPosition());
        }
    }

    // this might give a segfault in rare cases...
    for (auto& chunkPos : chunksToSave) {
        if (const Chunk* chunk = world.tryGetChunk(chunkPos)) {
            chunk->save();
        }
    }

    system->m_SaveInProgress = false;

    return nullptr;
}
