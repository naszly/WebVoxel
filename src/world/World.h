#pragma once

#include "Chunk.h"

class World {
public:
    World();
    ~World() = default;

    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;

    [[nodiscard]] std::vector<Vertex> getVoxelPoints() const {
        return m_Chunk.getPoints();
    }

private:
    Chunk<Voxel, 128, 128, 128> m_Chunk;
};