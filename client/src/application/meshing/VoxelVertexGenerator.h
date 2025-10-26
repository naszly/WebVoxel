#pragma once

#include <vector>
#include "application/types/VertexData.h"
#include "application/world/chunk/ChunkNeighborhood.h"

class VoxelVertexGenerator {
public:
    static void generate(const ChunkNeighborhood& neighborChunks, std::vector<VertexData>& vertices);

    static void generateDownsample2(const ChunkNeighborhood& neighborChunks,
                                    std::vector<VertexData>& vertices,
                                    float threshold = 1.0f);

    static void generateDownsample4(const ChunkNeighborhood& neighborChunks,
                                    std::vector<VertexData>& vertices,
                                    float threshold = 1.0f);

    static void generateDownsample8(const ChunkNeighborhood& neighborChunks,
                                    std::vector<VertexData>& vertices,
                                    float threshold = 1.0f);

private:
    template<size_t BlockSize>
    static void generateDownsample(const ChunkNeighborhood& neighborChunks,
                                   std::vector<VertexData>& vertices,
                                   float threshold);
};
