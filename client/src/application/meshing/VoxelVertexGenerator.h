#pragma once

#include <vector>
#include "application/graphics/VertexData.h"
#include "application/world/chunk/ChunkNeighborhood.h"

class VoxelVertexGenerator {
public:
    static void generate(const ChunkNeighborhood& neighborChunks, std::vector<VertexData>& vertices);
};
