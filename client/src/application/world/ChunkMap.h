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

    Chunk& createChunk(const glm::ivec3 &key);

    void insertChunkByMove(Chunk &chunk);

    [[nodiscard]] Chunk extractChunkByMove(const glm::ivec3& key);

    [[nodiscard]] auto getChunks() {
        return m_chunks | std::views::filter([](const auto& slot){ return slot.has_value(); })
                        | std::views::transform([](auto& slot) -> Chunk& { return *slot; });
    }

    [[nodiscard]] auto getChunks() const {
        return m_chunks | std::views::filter([](const auto& slot){ return slot.has_value(); })
                        | std::views::transform([](auto& slot) -> const Chunk& { return *slot; });
    }

    [[nodiscard]] size_t countChunks() const {
        return std::ranges::count_if(m_chunks, [](auto& slot){ return slot.has_value(); });
    }

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

    void setChunkDirty(const glm::ivec3& key);

    void setNeighboursDirty(const glm::ivec3& key);
};
