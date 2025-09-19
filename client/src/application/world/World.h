#pragma once

#include "ChunkMap.h"
#include "chunk/Chunk.h"
#include "chunk/ChunkNeighborhood.h"
#include "chunk/ChunkNeighborhoodPtrs.h"

class World {
public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;

    [[nodiscard]] auto getChunks() {
        return m_chunks.getChunks();
    }

    [[nodiscard]] auto getChunks() const {
        return m_chunks.getChunks();
    }

    [[nodiscard]] auto getChunksWithDirtyGpuBuffer() {
        auto chunks = m_chunks.getChunks();
        std::vector<std::reference_wrapper<Chunk>> dirtyChunks;
        for (auto& chunk : chunks) {
            if (chunk.isGpuBufferDirty()) {
                const auto neighborhood = getChunkNeighborhoodPtrs(chunk.getPosition());
                if (neighborhood.hasAllNeighbours()) {
                    dirtyChunks.push_back(chunk);
                }
            }
        }
        return dirtyChunks;
    }

    [[nodiscard]] Chunk* tryGetChunk(const glm::ivec3 &chunkPos) { return m_chunks.tryGetChunk(chunkPos); }

    [[nodiscard]] ChunkNeighborhoodPtrs getChunkNeighborhoodPtrs(const glm::ivec3 &chunkPos) const;

    [[nodiscard]] ChunkNeighborhood getChunkNeighborhood(const glm::ivec3 &chunkPos) const;

    [[nodiscard]] auto countChunks() const { return m_chunks.countChunks(); }

    [[nodiscard]] bool hasChunk(const glm::ivec3 &chunkPos) const;

    void createChunk(const glm::ivec3 &chunkPos);

    void insertChunkByMove(Chunk &chunk);

    [[nodiscard]] Chunk extractChunkByMove(const glm::ivec3& chunkPos);

    [[nodiscard]] VoxelData getVoxel(const WorldCoordinate &coord) const;

    [[nodiscard]] bool hasVoxel(const WorldCoordinate &coord) const;

    void setVoxel(const WorldCoordinate &coord, VoxelData voxel);

    void setVoxel(const WorldCoordinate &coord, VoxelData voxel, int64_t radius, bool isSphere = true);

    void removeVoxel(const WorldCoordinate &coord);

    void removeVoxel(const WorldCoordinate &coord, int64_t radius, bool isSphere = true);

    void clear() { m_chunks.clear(); }

private:
    ChunkMap m_chunks;
};
