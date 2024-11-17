#pragma once

#include "Chunk.h"
#include "ChunkMap.h"

class World {
public:
    constexpr static int CHUNKS = 2;

    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;

    void generate();

    [[nodiscard]] std::vector<std::pair<glm::ivec3, Chunk&>> getChunks();

    [[nodiscard]] VoxelData getVoxel(const WorldCoordinate &coord) {
        return m_Chunks.getVoxel(coord);
    }

    bool hasVoxel(const WorldCoordinate &coord) {
        return !m_Chunks.getVoxel(coord).isEmpty();
    }

    void setVoxel(const WorldCoordinate &coord, const VoxelData voxel) {
        m_Chunks.setVoxel(coord, voxel);
    }

    void removeVoxel(const WorldCoordinate &coord) {
        m_Chunks.setVoxel(coord, VoxelData{});
    }

private:
    ChunkMap m_Chunks;
};
