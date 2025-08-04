#pragma once

#include <cstdint>

struct VoxelData {
    uint32_t voxelId{0};

    VoxelData() = default;
    explicit VoxelData(const uint16_t blockId) : voxelId(blockId) {}

    [[nodiscard]] bool isEmpty() const {
        return voxelId == 0;
    }

    bool operator==(const VoxelData& other) const {
        return voxelId == other.voxelId;
    }

    bool operator!=(const VoxelData& other) const {
        return !(*this == other);
    }
};
