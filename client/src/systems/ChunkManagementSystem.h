#pragma once

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

    [[nodiscard]] bool isSaveInProgress() const { return m_SaveInProgress; }

    ~ChunkManagementSystem() override {
        m_ShouldExit = true;
    }

private:
    static constexpr int LOAD_RADIUS = 10;
    static constexpr int UNLOAD_RADIUS = 12;
    static_assert(LOAD_RADIUS < UNLOAD_RADIUS, "Load radius must be less than unload radius");

    const FastNoise::SmartNode<> m_fnGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");

    std::vector<glm::ivec3> m_LoadQueue;
    std::vector<Chunk> m_LoadedChunks;

    std::vector<std::unique_ptr<Threading::Worker>> m_LoadChunksWorkers{2};

    std::unique_ptr<Threading::Worker> m_SaveChunksWorker;

    Threading::Lock m_Lock;
    bool m_ShouldExit = false;

    bool m_SaveInProgress = false;

    void loadChunks(const Camera &camera, World &world);

    void updateLoadQueue(const Camera& camera, const World& world);

    static void unloadChunks(const Camera &camera, World &world);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    static void* worker(void *system);

    static void* saveAllChunksWorker(void *system);
};
