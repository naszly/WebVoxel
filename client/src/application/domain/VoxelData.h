#pragma once

#include "Block.h"
#include "BlockId.h"
#include "VoxelColor.h"

class VoxelData {
public:
    constexpr VoxelData() = default;
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b) : VoxelData{VoxelColor(r, g, b)} {}
    explicit VoxelData(const VoxelColor color) { setVoxelColor(color); }
    explicit VoxelData(const BlockId blockId) { setBlockId(blockId); }

    [[nodiscard]] VoxelColor getColor() const {
        const uint32_t packedColor = m_data;
        const uint8_t r = (packedColor >> 16) & 0xFF;
        const uint8_t g = (packedColor >> 8) & 0xFF;
        const uint8_t b = (packedColor >> 0) & 0xFF;
        return {r, g, b};
    }

    [[nodiscard]] BlockId getBlockId() const {
        return static_cast<BlockId>(m_data);
    }

    [[nodiscard]] Block getBlock() const {
        return Block(getBlockId());
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

    explicit operator uint32_t() const {
        return *reinterpret_cast<const uint32_t*>(this);
    }

    explicit VoxelData(const uint32_t data) {
        *reinterpret_cast<uint32_t*>(this) = data;
    }

    template <typename H>
    friend H AbslHashValue(H h, const VoxelData& v) {
        return H::combine(std::move(h), v.m_data, v.m_hasTexture);
    }

private:
    uint32_t m_data : 24 {0};
    bool m_hasTexture : 1 {false};
    bool m_emitsLight : 1 {false};

    void setVoxelColor(const VoxelColor color) {
        m_hasTexture = false;
        const uint32_t packedColor =
            (static_cast<uint32_t>(color.r) << 16) |
            (static_cast<uint32_t>(color.g) << 8) |
            (static_cast<uint32_t>(color.b) << 0);
        m_data = packedColor;
    }
    void setBlockId(const BlockId blockId) {
        m_hasTexture = true;
        m_data = static_cast<uint32_t>(blockId);
        m_emitsLight = Block(blockId).emitsLight();
    }
};

static_assert(sizeof(VoxelData) == 4, "VoxelData size must be 4 bytes");
