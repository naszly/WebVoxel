#pragma once

#include "System.h"
#include "../Thread.h"

#include <FastNoise/FastNoise.h>

class ChunkManagementSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    ~ChunkManagementSystem() override {
        m_ShouldExit = true;
    }

private:
    static constexpr int m_LoadRadius = 8;
    static constexpr int m_UnloadRadius = 10;
    static_assert(m_LoadRadius < m_UnloadRadius, "Load radius must be less than unload radius");

    const FastNoise::SmartNode<> fnGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");

    std::vector<glm::ivec3> m_LoadQueue;
    std::vector<Chunk> m_LoadedChunks;

    std::vector<std::unique_ptr<Threading::Worker>> m_LoadChunksWorkers{2};

    Threading::Lock m_Lock;
    bool m_ShouldExit = false;

    void loadChunks(const Camera &camera, World &world);

    void updateLoadQueue(const Camera& camera, const World& world);

    static void unloadChunks(const Camera &camera, World &world);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    static void* worker(void *system);
};
