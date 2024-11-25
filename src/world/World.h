#pragma once

#include "Chunk.h"
#include "ChunkMap.h"

class World {
public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;

    void update(glm::vec3 playerPosition);

    [[nodiscard]] std::vector<ChunkVertexBuffer> getChunkVertexBuffers();

    [[nodiscard]] VoxelData getVoxel(const WorldCoordinate &coord) const;

    [[nodiscard]] bool hasVoxel(const WorldCoordinate &coord) const;

    void setVoxel(const WorldCoordinate &coord, VoxelData voxel);

    void setVoxel(const WorldCoordinate &coord, VoxelData voxel, int64_t radius, bool isSphere = true);

    void removeVoxel(const WorldCoordinate &coord);

    void removeVoxel(const WorldCoordinate &coord, int64_t radius, bool isSphere = true);

private:
    ChunkMap m_Chunks;

    ChunkNeighbours getChunkNeighbours(const glm::ivec3 &chunkPos);
};
