#pragma once

#include "../domain/VoxelData.h"
#include "AmbientOcclusion.h"

struct PackedLight {
    uint32_t packedIntensities{0};

    PackedLight() = default;
    explicit PackedLight(const BlockLightInfo& faceNxLightInfo, const BlockLightInfo& facePxLightInfo, const BlockLightInfo& faceNyLightInfo, const BlockLightInfo& facePyLightInfo, const BlockLightInfo& faceNzLightInfo, const BlockLightInfo& facePzLightInfo) {
        // 5 bits per face, 6 faces = 30 bits total
        packedIntensities =
            (static_cast<uint32_t>(faceNxLightInfo.getIntensity() & 0x1F) << 0) |
            (static_cast<uint32_t>(facePxLightInfo.getIntensity() & 0x1F) << 5) |
            (static_cast<uint32_t>(faceNyLightInfo.getIntensity() & 0x1F) << 10) |
            (static_cast<uint32_t>(facePyLightInfo.getIntensity() & 0x1F) << 15) |
            (static_cast<uint32_t>(faceNzLightInfo.getIntensity() & 0x1F) << 20) |
            (static_cast<uint32_t>(facePzLightInfo.getIntensity() & 0x1F) << 25);
    }
};


struct VertexData {
    uint8_t x{}, y{}, z{}, w{};
    VoxelData voxel;
    AmbientOcclusion ambientOcclusion{};
    PackedLight light{};
};
static_assert(sizeof(VertexData) == 16, "VertexData size must be 16 bytes");