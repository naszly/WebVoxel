#pragma once

#include <vector>
#include <queue>

#include "System.h"
#include "application/world/WorldGenerator.h"
#include "common/datastructures/HashSet.h"
#include "common/Thread.h"

class ChunkManagementSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    explicit ChunkManagementSystem(const bool cavesEnabled = true)
        : System(), m_generator(cavesEnabled) {}

    ~ChunkManagementSystem() override {
        m_shouldExit = true;
        m_chunkWorkers.clear();
    }

private:
    struct CompressionTask {
        glm::ivec3 position;
        Chunk chunk;
        std::chrono::steady_clock::time_point lastAccess;
    };

    struct Work {
        std::optional<Chunk> chunkToSave = std::nullopt;
        std::optional<glm::ivec3> chunkToLoad = std::nullopt;
        std::queue<Chunk> chunksToUnload;
        std::optional<CompressionTask> compressionTask = std::nullopt;

        [[nodiscard]] bool hasPendingWork() const {
            return !chunksToUnload.empty() || chunkToSave.has_value() || chunkToLoad.has_value() || compressionTask.has_value();
        }
    };

    static constexpr int FAST_ACCESS_RADIUS = 8;
    static constexpr int LOAD_ZONE_RADIUS_XZ = 20;
    static constexpr int LOAD_ZONE_RADIUS_Y = 12;
    static constexpr int UNLOAD_ZONE_RADIUS_XZ = LOAD_ZONE_RADIUS_XZ + 1;
    static constexpr int UNLOAD_ZONE_RADIUS_Y = LOAD_ZONE_RADIUS_Y + 1;

    std::queue<glm::ivec3> m_chunksToLoad;
    HashSet<glm::ivec3> m_loadingChunks;
    std::vector<Chunk> m_loadedChunks;

    std::queue<Chunk> m_chunksToSave;
    HashSet<glm::ivec3> m_savingChunks;

    std::queue<Chunk> m_chunksToUnload;

    std::queue<CompressionTask> m_chunksToCompress;
    HashSet<glm::ivec3> m_compressingChunks;
    std::vector<CompressionTask> m_compressedChunks;

    size_t m_chunkWorkersCount{};
    std::vector<std::unique_ptr<Threading::Worker>> m_chunkWorkers{};

    Threading::Lock m_lock;
    bool m_shouldExit = false;

    WorldGenerator m_generator;

    void processChunkManagement(const Camera& camera, World& world);

    void integrateLoadedChunks(World &world);
    void integrateCompressedChunks(World& world);

    static std::vector<glm::ivec3> generateChunkOffsets();

    void scheduleChunksForLoading(const Camera& camera, const World& world);
    void scheduleChunksForSaving(World &world);
    void scheduleChunksForUnloading(const Camera &camera, World &world);
    void scheduleChunksForCompression(World& world, const Camera& camera);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    void handleChunkSave(std::optional<Chunk>& chunkToSave);
    void handleChunkLoad(std::optional<glm::ivec3>& chunkToLoad);
    void handleChunkCompression(std::optional<CompressionTask>& task);

    static void* worker(void *arg);

    bool fetchWork(Work& work);
};
