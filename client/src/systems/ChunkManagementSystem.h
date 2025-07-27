#pragma once

#include <vector>
#include <queue>
#include <unordered_set>

#include "core/System.h"
#include "common/Thread.h"

#include <FastNoise/FastNoise.h>

class ChunkManagementSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    void saveAllChunks();

    [[nodiscard]] bool isSaveInProgress() const { return m_saveInProgress; }

    ~ChunkManagementSystem() override {
        m_shouldExit = true;
    }

private:
    static constexpr int LOAD_RADIUS = 10;
    static constexpr int UNLOAD_RADIUS = 12;
    static_assert(LOAD_RADIUS < UNLOAD_RADIUS, "Load radius must be less than unload radius");

    const FastNoise::SmartNode<> m_fnGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");

    std::queue<glm::ivec3> m_chunksToLoad;
    std::unordered_set<glm::ivec3> m_loadingChunks;
    std::vector<Chunk> m_loadedChunks;

    const size_t m_loadChunksWorkersCount = 2;
    std::vector<std::unique_ptr<Threading::Worker>> m_loadChunksWorkers{m_loadChunksWorkersCount};

    std::unique_ptr<Threading::Worker> m_saveChunksWorker;

    Threading::Lock m_lock;
    bool m_shouldExit = false;

    bool m_saveInProgress = false;

    void loadChunks(const Camera &camera, World &world);

    static std::vector<glm::ivec3> generateChunkOffsets();

    void updateLoadQueue(const Camera& camera, const World& world);

    static void unloadChunks(const Camera &camera, World &world);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    static void* worker(void *arg);

    static void* saveAllChunksWorker(void *arg);
};
