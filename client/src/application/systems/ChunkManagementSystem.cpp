#include "ChunkManagementSystem.h"

void ChunkManagementSystem::initialize() {

    const auto hardwareConcurrency = Threading::hardwareConcurrency();

    m_chunkWorkersCount = 1;

    if (hardwareConcurrency > 2) {
        m_chunkWorkersCount = hardwareConcurrency / 2;
    }

    for (size_t i = 0; i < m_chunkWorkersCount; ++i) {
        m_chunkWorkers.push_back(std::make_unique<Threading::Worker>());
        m_chunkWorkers[i]->start(worker, this);
    }
}

void ChunkManagementSystem::update(float dt) {
    const Camera& camera = getCamera();
    World& world = getWorld();

    integrateLoadedChunks(world);

    scheduleChunksForLoading(camera, world);

    scheduleChunksForSaving(world);

    scheduleChunksForUnloading(camera, world);
}

void ChunkManagementSystem::integrateLoadedChunks(World &world) {
    Threading::ScopedLock lock(&m_lock);

    for (auto& chunk : m_loadedChunks) {
        world.insertChunkByMove(chunk);
    }
    m_loadedChunks.clear();
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

void ChunkManagementSystem::scheduleChunksForLoading(const Camera& camera, const World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    static std::vector<glm::ivec3> offsets = generateChunkOffsets();

    std::vector<glm::ivec3> chunksToLoad;
    const size_t maxChunksToLoad = m_chunkWorkersCount * 3;

    Threading::ScopedLock lock(&m_lock);

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
            Threading::ScopedLock lock(&m_lock);
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

    const auto chunks = world.getChunks();

    std::vector<glm::ivec3> chunksToUnload;

    for (const auto &chunk : chunks) {
        auto chunkPos = chunk.getPosition();
        if (getChunkDistance(playerPosition, chunkPos) > UNLOAD_RADIUS) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    Threading::ScopedLock lock(&m_lock);

    for (const auto &chunkPos : chunksToUnload) {
        m_chunksToUnload.push(world.extractChunkByMove(chunkPos));
    }
}

float ChunkManagementSystem::getChunkDistance(const glm::vec3 playerPosition, const glm::ivec3 chunkPos) {
    const auto transformedPlayerPosition = playerPosition * glm::vec3(1.0f, 1.33f, 1.0f);
    const auto transformedChunkPos = glm::vec3(chunkPos) * glm::vec3(1.0f, 1.33f, 1.0f);
    return glm::distance(glm::vec3(transformedChunkPos), glm::vec3(WorldCoordinate(transformedPlayerPosition).chunkPosition()));
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

void ChunkManagementSystem::handleChunkLoad(std::optional<glm::ivec3>& chunkToLoad, const FastNoise::SmartNode<>& fnGenerator) {
    if (!chunkToLoad.has_value()) return;

    const auto chunkPos = chunkToLoad.value();
    Chunk chunk(chunkPos);
    if (chunk.fileExists()) {
        chunk.load();
    } else {
        chunk.generate(fnGenerator);
    }
    chunkToLoad = std::nullopt;

    Threading::ScopedLock lock(&m_lock);
    m_loadedChunks.emplace_back(std::move(chunk));
    m_loadingChunks.erase(chunkPos);
}

bool ChunkManagementSystem::fetchWork(Work& work) {
    Threading::ScopedLock lock(&m_lock);

    if (m_shouldExit && m_chunksToSave.empty()) {
        return false;
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

    return true;
}

void* ChunkManagementSystem::worker(void *arg) {
    auto& system = *static_cast<ChunkManagementSystem*>(arg);
    const FastNoise::SmartNode<> fnGenerator = FastNoise::NewFromEncodedNodeTree(system.m_fnGeneratorEncoded);

    Work work;

    while (system.fetchWork(work)) {

        while (!work.chunksToUnload.empty()) {
            work.chunksToUnload.pop();
        }

        if (!work.hasPendingWork()) {
            Threading::sleep(200);
            continue;
        }

        if (work.chunkToSave.has_value()) {
            system.handleChunkSave(work.chunkToSave);
        }

        if (work.chunkToLoad.has_value()) {
            system.handleChunkLoad(work.chunkToLoad, fnGenerator);
        }
    }

    return nullptr;
}
