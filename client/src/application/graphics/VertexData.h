#pragma once

#include "../domain/VoxelData.h"
#include "AmbientOcclusion.h"

struct VertexData {
    uint8_t x{}, y{}, z{}, w{};
    VoxelData voxel;
};

struct VertexDataAo : VertexData {
    AmbientOcclusion ambientOcclusion{};
};