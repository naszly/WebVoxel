#pragma once

#include <ranges>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

#include "chunk/Chunk.h"
#include "WorldCoordinate.h"

class ChunkMap {
public:
    ChunkMap() = default;
    ~ChunkMap() = default;

    ChunkMap(const ChunkMap&) = delete;
    ChunkMap(ChunkMap&&) = delete;
    ChunkMap& operator=(const ChunkMap&) = delete;
    ChunkMap& operator=(ChunkMap&&) = delete;

    [[nodiscard]] Chunk& getChunk(glm::ivec3 key);

    [[nodiscard]] Chunk* tryGetChunk(glm::ivec3 key);

    [[nodiscard]] const Chunk* tryGetChunk(glm::ivec3 key) const;

    [[nodiscard]] bool hasChunk(glm::ivec3 key) const;

    Chunk& createChunk(const glm::ivec3 &key);

    void insertChunkByMove(Chunk &chunk);

    Chunk extractChunkByMove(const glm::ivec3& key);

    [[nodiscard]] auto getChunks() { return m_chunks | std::ranges::views::values; }

    [[nodiscard]] size_t countChunks() const { return m_chunks.size(); }

    [[nodiscard]] VoxelData getVoxel(const WorldCoordinate &coord) const;

    [[nodiscard]] bool hasVoxel(const WorldCoordinate &coord) const;

    void setVoxel(const WorldCoordinate &coord, const VoxelData &voxel);

    void removeVoxel(const WorldCoordinate &coord);

private:
    std::unordered_map<glm::ivec3, Chunk> m_chunks;

    void setChunkDirty(const glm::ivec3& key);

    void setNeighboursDirty(const glm::ivec3& key);

    void setNeighboursDirtyIfEdge(const glm::ivec3& key, const glm::ivec3& pos);
};
