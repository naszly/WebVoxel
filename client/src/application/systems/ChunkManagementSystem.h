#pragma once

#include <vector>
#include <queue>

#include "System.h"
#include "common/datastructures/HashSet.h"
#include "common/Thread.h"

class ChunkManagementSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    ~ChunkManagementSystem() override {
        m_shouldExit = true;
    }

private:
    struct Work {
        std::optional<Chunk> chunkToSave = std::nullopt;
        std::optional<glm::ivec3> chunkToLoad = std::nullopt;
        std::queue<Chunk> chunksToUnload;

        [[nodiscard]] bool hasPendingWork() const {
            return !chunksToUnload.empty() || chunkToSave.has_value() || chunkToLoad.has_value();
        }
    };

    static constexpr int LOAD_RADIUS = 10;
    static constexpr int UNLOAD_RADIUS = 12;
    static_assert(LOAD_RADIUS < UNLOAD_RADIUS, "Load radius must be less than unload radius");

    const char* m_fnGeneratorEncoded = "DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==";

    std::queue<glm::ivec3> m_chunksToLoad;
    HashSet<glm::ivec3> m_loadingChunks;
    std::vector<Chunk> m_loadedChunks;

    std::queue<Chunk> m_chunksToSave;
    HashSet<glm::ivec3> m_savingChunks;

    std::queue<Chunk> m_chunksToUnload;

    size_t m_chunkWorkersCount{};
    std::vector<std::unique_ptr<Threading::Worker>> m_chunkWorkers{};

    Threading::Lock m_lock;
    bool m_shouldExit = false;

    void integrateLoadedChunks(World &world);

    static std::vector<glm::ivec3> generateChunkOffsets();

    void scheduleChunksForLoading(const Camera& camera, const World& world);

    void scheduleChunksForSaving(World &world);

    void scheduleChunksForUnloading(const Camera &camera, World &world);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    static void* worker(void *arg);

    void handleChunkSave(std::optional<Chunk>& chunkToSave);

    void handleChunkLoad(std::optional<glm::ivec3>& chunkToLoad, const FastNoise::SmartNode<>& fnGenerator);

    bool fetchWork(Work& work);
};
