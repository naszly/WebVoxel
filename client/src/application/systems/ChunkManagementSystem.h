#pragma once

#include <vector>
#include <queue>

#include "System.h"
#include "application/graphics/types/ChunkVertexBuffer.h"
#include "application/world/WorldGenerator.h"
#include "common/datastructures/HashSet.h"
#include "common/Thread.h"

class ChunkManagementSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override {}

    void update(float dt) override;

    void onEvent(Event &event) override {}

    explicit ChunkManagementSystem(
        const std::string& path,
        std::optional<WorldGeneratorParams> worldGeneratorParam = std::nullopt,
        std::string roomCode = {},
        std::string roomToken = {})
        : System(), m_roomCode(std::move(roomCode)), m_roomToken(std::move(roomToken)) {
        m_savePath = path;
        if (worldGeneratorParam) {
            saveWorldGeneratorParams(m_savePath, worldGeneratorParam.value());
        } else {
            worldGeneratorParam = loadWorldGeneratorParams(m_savePath);
        }
        m_generatorParams = *worldGeneratorParam;
        m_generator = std::make_unique<WorldGenerator>(m_generatorParams);
    }

    ~ChunkManagementSystem() override {
        m_shouldExit = true;
        m_chunkWorkers.clear();
    }

    // Save and load player state (position & direction) to the world save path
    void savePlayerState(const Camera& camera);
    bool loadPlayerState(Camera& camera);

    [[nodiscard]] bool isMultiplayer() const { return !m_roomCode.empty(); }

    void setLoadDistance(const int distance) {
        if (distance == m_loadDistance) {
            return;
        }
        m_loadDistance = distance;
        m_fastAccessRadius = m_loadDistance / 80;
        m_loadZoneRadiusXz = m_loadDistance / 32;
        m_loadZoneRadiusY = m_loadDistance / 48;
        m_unloadZoneRadiusXz = m_loadZoneRadiusXz + 1;
        m_unloadZoneRadiusY = m_loadZoneRadiusY + 1;
        m_chunkOffsets = generateChunkOffsets();
    }

    [[nodiscard]] int getLoadDistance() const {
        return m_loadDistance;
    }

private:
    // Enhances move performance by transferring only pointers,
    // avoiding costly deep moves or copies of Chunk objects.
    struct ChunkHandle {
        std::unique_ptr<Chunk> chunk;
        explicit ChunkHandle(Chunk&& c) : chunk(std::make_unique<Chunk>(std::move(c))) {}
        ChunkHandle(const ChunkHandle&) = delete;
        ChunkHandle& operator=(const ChunkHandle&) = delete;
        ChunkHandle(ChunkHandle&&) = default;
        ChunkHandle& operator=(ChunkHandle&&) = default;

        static ChunkHandle makeCopy(const Chunk& c) {
            return ChunkHandle(Chunk(c));
        }

        Chunk& operator*() { return *chunk; }
        const Chunk& operator*() const { return *chunk; }
        Chunk* operator->() { return chunk.get(); }
        const Chunk* operator->() const { return chunk.get(); }
    };

    struct CompressionTask {
        glm::ivec3 position;
        ChunkHandle chunk;
        std::chrono::steady_clock::time_point lastAccess;
    };

    struct Work {
        std::optional<ChunkHandle> chunkToSave = std::nullopt;
        std::optional<glm::ivec3> chunkToLoad = std::nullopt;
        std::queue<ChunkHandle> chunksToUnload;
        std::optional<CompressionTask> compressionTask = std::nullopt;

        [[nodiscard]] bool hasPendingWork() const {
            return !chunksToUnload.empty()
                || chunkToSave.has_value()
                || chunkToLoad.has_value()
                || compressionTask.has_value();
        }
    };

    int m_loadDistance{};
    int m_fastAccessRadius{};
    int m_loadZoneRadiusXz{};
    int m_loadZoneRadiusY{};
    int m_unloadZoneRadiusXz{};
    int m_unloadZoneRadiusY{};
    std::vector<glm::ivec3> m_chunkOffsets;

    std::queue<glm::ivec3> m_chunksToLoad;
    HashSet<glm::ivec3> m_loadingChunks;
    std::vector<ChunkHandle> m_loadedChunks;

    std::queue<ChunkHandle> m_chunksToSave;
    HashSet<glm::ivec3> m_savingChunks;

    std::queue<ChunkHandle> m_chunksToUnload;

    std::queue<CompressionTask> m_chunksToCompress;
    HashSet<glm::ivec3> m_compressingChunks;
    std::vector<CompressionTask> m_compressedChunks;

    size_t m_chunkWorkersCount{};
    std::vector<std::unique_ptr<Threading::Worker>> m_chunkWorkers{};

    Threading::Lock m_lock;
    bool m_shouldExit = false;

    std::unique_ptr<WorldGenerator> m_generator{};
    WorldGeneratorParams m_generatorParams{};
    std::string m_savePath{};
    const std::string m_roomCode{};
    const std::string m_roomToken{};

    void processChunkManagement(const Camera& camera, World& world);

    void integrateLoadedChunks(World &world);
    void integrateCompressedChunks(World& world);

    [[nodiscard]] std::vector<glm::ivec3> generateChunkOffsets() const;

    void scheduleChunksForLoading(const Camera& camera, const World& world);
    void scheduleChunksForSaving(World &world);
    void scheduleChunksForUnloading(const Camera &camera, World &world);
    void scheduleChunksForCompression(World& world, const Camera& camera);

    static float getChunkDistance(glm::vec3 playerPosition, glm::ivec3 chunkPos);

    void handleChunkSave(ChunkHandle& chunkToSave);
    void handleChunkLoad(const glm::ivec3& chunkToLoad);
    void handleChunkCompression(CompressionTask& task);

    static void* worker(void *arg);

    bool fetchWork(Work& work);

    static void saveWorldGeneratorParams(const std::string &path, WorldGeneratorParams worldGeneratorParams);

    static WorldGeneratorParams loadWorldGeneratorParams(const std::string &path);
};
