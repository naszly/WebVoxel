#pragma once

#include <cstdint>
#include "application/graphics/AmbientOcclusion.h"
#include "application/world/chunk/ChunkNeighborhood.h"

class AmbientOcclusionComputer {
public:
    static AmbientOcclusion compute(const ChunkNeighborhood& neighborChunks, uint32_t x, uint32_t y, uint32_t z);
};

