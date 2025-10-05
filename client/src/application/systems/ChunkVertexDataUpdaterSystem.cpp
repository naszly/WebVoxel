#include "ChunkVertexDataUpdaterSystem.h"

#include "RendererSystem.h"
#include "application/Application.h"
#include "application/meshing/VoxelVertexGenerator.h"

void ChunkVertexDataUpdaterSystem::initialize() {
    m_workersCount = 1;

    for (size_t i = 0; i < m_workersCount; ++i) {
        m_workers.push_back(std::make_unique<Threading::Worker>());
        m_workers[i]->start(worker, this);
    }
}

void ChunkVertexDataUpdaterSystem::update(float dt) {
    integrateCreatedChunkVertexData();
    scheduleChunksForVertexDataCreation(getCamera(), getWorld());
}

void ChunkVertexDataUpdaterSystem::integrateCreatedChunkVertexData() {
    const auto rendererSystem = getApplication().getSystem<RendererSystem>();

    std::queue<std::pair<glm::ivec3, std::vector<VertexData>>> updates;
    std::queue<ChunkNeighborhood> freeNeighborhoods;
    {
        Threading::ScopedLock lock(&m_lock);

        while (!m_dirtyChunkVertexDatas.empty()) {
            updates.push(std::move(m_dirtyChunkVertexDatas.front()));
            m_dirtyChunkVertexDatas.pop();
        }

        while (!m_freeChunkNeighborhoods.empty()) {
            freeNeighborhoods.push(std::move(m_freeChunkNeighborhoods.front()));
            m_freeChunkNeighborhoods.pop();
        }
    }

    if (rendererSystem) {
        while (!updates.empty()) {
            auto& [chunkPos, vertexData] = updates.front();
            rendererSystem->updateChunkVertexBuffer(vertexData, chunkPos);
            updates.pop();
        }
    }

    // free old ChunkNeighborhoods on the same thread they were allocated
    while (!freeNeighborhoods.empty()) {
        freeNeighborhoods.pop();
    }

}

void ChunkVertexDataUpdaterSystem::scheduleChunksForVertexDataCreation(const Camera& camera, World& world) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();
    auto dirty = world.getChunksWithDirtyGpuBuffer();

    std::vector<std::reference_wrapper<Chunk>> emptyChunks;
    std::vector<std::reference_wrapper<Chunk>> nonEmptyChunks;
    for (auto& chunkRef : dirty) {
        if (chunkRef.get().isEmpty()) {
            emptyChunks.emplace_back(chunkRef);
        } else {
            nonEmptyChunks.emplace_back(chunkRef);
        }
    }

    const auto rendererSystem = getApplication().getSystem<RendererSystem>();
    for (auto& chunkRef : emptyChunks) {
        auto& chunk = chunkRef.get();
        constexpr std::vector<VertexData> emptyVertexData;
        const auto& chunkPosition = chunk.getPosition();
        rendererSystem->updateChunkVertexBuffer(emptyVertexData, chunkPosition);
        chunk.resetGpuBufferDirty();
    }

    std::ranges::sort(nonEmptyChunks, [&](const Chunk &a, const Chunk &b) {
        const auto aPos = a.getPosition();
        const auto bPos = b.getPosition();
        return Utils::distance2(aPos, playerChunk) < Utils::distance2(bPos, playerChunk);
    });

    Threading::ScopedLock lock(&m_lock);
    for (auto& chunkRef : nonEmptyChunks) {
        auto& chunk = chunkRef.get();
        auto position = chunk.getPosition();
        auto chunkNeighborhood = world.getChunkNeighborhood(position);
        if (!chunkNeighborhood.hasAllNeighbours()) {
            continue;
        }
        if (!m_dirtyChunks.push(std::move(chunkNeighborhood))) {
            break;
        }
        chunk.resetGpuBufferDirty();
    }
}

void* ChunkVertexDataUpdaterSystem::worker(void* arg) {
    auto& system = *static_cast<ChunkVertexDataUpdaterSystem*>(arg);

    Work work;

    while (system.fetchWork(work)) {

        if (!work.hasPendingWork()) {
            Threading::sleep(10);
            continue;
        }

        while (!work.vertexDataToCreate.empty()) {
            auto chunkNeighborhood = work.vertexDataToCreate.pop();
            const Chunk* chunk = chunkNeighborhood.getCenterChunk();

            const auto position = chunk->getPosition();
            if (!chunkNeighborhood.hasAllNeighbours()) {
                continue;
            }

            thread_local std::vector<VertexData> points;
            points.clear();
            VoxelVertexGenerator::generate(chunkNeighborhood, points);

            work.createdVertexData.push({
                position,
                std::move(points),
                std::move(chunkNeighborhood)
            });
        }
    }

    return nullptr;
}

bool ChunkVertexDataUpdaterSystem::fetchWork(Work& work) {

    if (m_shouldExit) {
        return false;
    }

    Threading::ScopedLock lock(&m_lock);

    while (auto item = std::move(work.createdVertexData.tryPop())) {
        m_dirtyChunkVertexDatas.emplace(item->chunkNeighborPosition, std::move(item->vertexData));
        m_freeChunkNeighborhoods.push(std::move(item->chunkNeighborhoodToFree));
    }

    if (work.vertexDataToCreate.size() < work.vertexDataToCreate.capacity() / 2) {
        while (!work.vertexDataToCreate.full()) {
            if (auto item = std::move(m_dirtyChunks.tryPop())) {
                work.vertexDataToCreate.push(std::move(*item));
            } else {
                break;
            }
        }
    }

    return true;
}
