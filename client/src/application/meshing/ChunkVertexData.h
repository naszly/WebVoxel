#pragma once

#include <vector>

#include "VoxelVertexGenerator.h"
#include "application/types/VertexData.h"
#include "application/world/chunk/ChunkNeighborhood.h"

struct ChunkVertexData {
    std::vector<VertexData> fullResolution;
    std::vector<VertexData> downsampledBy2;
    std::vector<VertexData> downsampledBy4;
    std::vector<VertexData> downsampledBy8;

    ChunkVertexData() = default;

    explicit ChunkVertexData(const ChunkNeighborhood &chunkNeighborhood) {
        VoxelVertexGenerator::generate(chunkNeighborhood, fullResolution);
        VoxelVertexGenerator::generateDownsample2(chunkNeighborhood, downsampledBy2, 0.9f);
        VoxelVertexGenerator::generateDownsample4(chunkNeighborhood, downsampledBy4, 0.9f);
        VoxelVertexGenerator::generateDownsample8(chunkNeighborhood, downsampledBy8, 0.9f);
    }
};
