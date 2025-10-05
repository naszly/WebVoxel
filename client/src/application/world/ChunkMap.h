#pragma once

#include <ranges>
#include <vector>
#include <optional>
#include <glm/glm.hpp>
#include "chunk/Chunk.h"
#include "WorldCoordinate.h"

class ChunkMap {
public:
    static constexpr int SIZE = 64;
    static constexpr int TOTAL_SIZE = SIZE * SIZE * SIZE;

    ChunkMap() : m_chunks(TOTAL_SIZE) {}
    ~ChunkMap() = default;

    ChunkMap(const ChunkMap&) = delete;
    ChunkMap(ChunkMap&&) = delete;
    ChunkMap& operator=(const ChunkMap&) = delete;
    ChunkMap& operator=(ChunkMap&&) = delete;

    [[nodiscard]] Chunk& getChunk(glm::ivec3 key);

    [[nodiscard]] std::optional<Chunk>& tryGetChunk(glm::ivec3 key);

    [[nodiscard]] const std::optional<Chunk>& tryGetChunk(glm::ivec3 key) const;

    [[nodiscard]] Chunk* tryGetChunkPtr(glm::ivec3 key);

    [[nodiscard]] const Chunk* tryGetChunkPtr(glm::ivec3 key) const;

    [[nodiscard]] bool hasChunk(glm::ivec3 key) const;

    bool areChunkAndNeighborsPresent(const glm::ivec3& key) const;

    Chunk& createChunk(const glm::ivec3 &key);

    void insertChunkByMove(Chunk &chunk);

    [[nodiscard]] Chunk extractChunkByMove(const glm::ivec3& key);

    [[nodiscard]] std::vector<std::reference_wrapper<Chunk>> getChunks();

    [[nodiscard]] std::vector<std::reference_wrapper<const Chunk>> getChunks() const;

    [[nodiscard]] std::vector<std::reference_wrapper<Chunk>> getChunksWithDirtyGpuBuffer();

    [[nodiscard]] VoxelData getVoxel(const WorldCoordinate &coord) const;

    [[nodiscard]] bool hasVoxel(const WorldCoordinate &coord) const;

    void setVoxel(const WorldCoordinate &coord, const VoxelData &voxel);

    Chunk* queueVoxelToSet(const WorldCoordinate& coord, const VoxelData& voxel);

    void executeQueuedVoxelsToSet(Chunk* chunk);

    void clear();

private:
    static uint32_t packIndex(const glm::ivec3& key) noexcept {
        return (key.x & 0x3F) | ((key.y & 0x3F) << 6) | ((key.z & 0x3F) << 12);
    }

    std::vector<std::optional<Chunk>> m_chunks;
    Bitmap<TOTAL_SIZE> m_chunkBitmap;

    void setChunkDirty(const glm::ivec3& key);

    void setNeighboursDirty(const glm::ivec3& key);

    template<typename Func>
    void forEachChunk(Func&& func) const {
        for (uint32_t word = 0; word < TOTAL_SIZE / decltype(m_chunkBitmap)::BITS_PER_WORD; ++word) {
            if (!m_chunkBitmap.testWord(word)) continue;
            for (uint32_t bit = 0; bit < decltype(m_chunkBitmap)::BITS_PER_WORD; ++bit) {
                const uint32_t i = word * decltype(m_chunkBitmap)::BITS_PER_WORD + bit;
                if (i >= TOTAL_SIZE) break;
                if (m_chunkBitmap.test(i)) {
                    func(i);
                }
            }
        }
    }

};
