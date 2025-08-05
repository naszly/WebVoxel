#pragma once

#include "BlockId.h"
#include "VoxelColor.h"

class VoxelData {
public:
    constexpr VoxelData() : m_data(0), m_hasTexture(false) {}
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b) : VoxelData{VoxelColor(r, g, b)} {}
    explicit VoxelData(const VoxelColor color) : m_hasTexture(false) { setVoxelColor(color); }
    explicit VoxelData(const BlockId blockId) : m_hasTexture(true) { setBlockId(blockId); }

    VoxelColor getColor() const {
        const uint32_t packedColor = m_data;
        const uint8_t r = (packedColor >> 16) & 0xFF;
        const uint8_t g = (packedColor >> 8) & 0xFF;
        const uint8_t b = (packedColor >> 0) & 0xFF;
        return VoxelColor(r, g, b);
    }

    BlockId getBlockId() const {
        return static_cast<BlockId>(m_data);
    }

    [[nodiscard]] bool isEmpty() const {
        return m_data == 0;
    }

    bool operator==(const VoxelData& other) const {
        return m_data == other.m_data && m_hasTexture == other.m_hasTexture;
    }

    bool operator!=(const VoxelData& other) const {
        return !(*this == other);
    }

    template <typename H>
    friend H AbslHashValue(H h, const VoxelData& v) {
        return H::combine(std::move(h), v.m_data, v.m_hasTexture);
    }

private:
    uint32_t m_data : 24;
    bool m_hasTexture : 1;

    void setVoxelColor(const VoxelColor color) {
        const uint32_t packedColor =
            (static_cast<uint32_t>(color.r) << 16) |
            (static_cast<uint32_t>(color.g) << 8) |
            (static_cast<uint32_t>(color.b) << 0);
        m_data = packedColor;
    }
    void setBlockId(const BlockId blockId) {
        m_data = static_cast<uint32_t>(blockId);
    }
};

static_assert(sizeof(VoxelData) == 4, "VoxelData size must be 4 bytes");
