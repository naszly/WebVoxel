#pragma once

#include "AmbientOcclusion.h"
#include "PackedLight.h"
#include "application/domain/VoxelData.h"

struct VertexData {
    uint8_t x{}, y{}, z{}, w{};
    VoxelData voxel;
    AmbientOcclusion ambientOcclusion{};
    PackedLight light{};
};
static_assert(sizeof(VertexData) == 16, "VertexData size must be 16 bytes");