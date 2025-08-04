#pragma once

#include "BlockId.h"

struct VoxelData {
    BlockId blockId{0};

    VoxelData() = default;
    explicit VoxelData(const BlockId blockId) : blockId(blockId) {}
    explicit VoxelData(const uint32_t id) : blockId(static_cast<BlockId>(id)) {}

    [[nodiscard]] bool isEmpty() const {
        return blockId == BlockId::Air;
    }

    bool operator==(const VoxelData& other) const {
        return blockId == other.blockId;
    }

    bool operator!=(const VoxelData& other) const {
        return !(*this == other);
    }
};
