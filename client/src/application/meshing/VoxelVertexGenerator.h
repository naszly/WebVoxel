#pragma once

#include <vector>
#include "application/graphics/types/VertexData.h"
#include "application/world/chunk/ChunkNeighborhood.h"

class VoxelVertexGenerator {
public:
    static void generate(const ChunkNeighborhood& neighborChunks, std::vector<VertexData>& vertices);
};
