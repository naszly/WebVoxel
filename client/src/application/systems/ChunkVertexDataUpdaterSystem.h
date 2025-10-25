#pragma once

#include <queue>

#include "System.h"
#include "application/meshing/ChunkVertexData.h"

class ChunkVertexDataUpdaterSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    ChunkVertexDataUpdaterSystem() : System() {}

    ~ChunkVertexDataUpdaterSystem() override {
        m_shouldExit = true;
        m_workers.clear();
    }

private:
    struct Work {
        CircularBuffer<ChunkNeighborhood, 24> vertexDataToCreate;
        struct Result {
            glm::ivec3 chunkNeighborPosition;
            ChunkVertexData vertexData;
            ChunkNeighborhood chunkNeighborhoodToFree;
        };
        CircularBuffer<Result, 24> createdVertexData;

        [[nodiscard]] bool hasPendingWork() const {
            return !vertexDataToCreate.empty();
        }
    };

    CircularBuffer<ChunkNeighborhood, 128> m_dirtyChunks;
    std::queue<std::pair<glm::ivec3, ChunkVertexData>> m_dirtyChunkVertexDatas;
    std::queue<ChunkNeighborhood> m_freeChunkNeighborhoods;

    size_t m_workersCount{};
    std::vector<std::unique_ptr<Threading::Worker>> m_workers{};

    Threading::Lock m_lock;
    bool m_shouldExit = false;

    void integrateCreatedChunkVertexData();

    void scheduleChunksForVertexDataCreation(const Camera &camera, World& world);

    static void* worker(void *arg);

    bool fetchWork(Work& work);
};
