#pragma once

#include <cstdint>

enum class BlockId : uint32_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    Duskstone = 4,
    Blackrock = 5,
    EclipseCrystal = 6,
    MoonlitLantern = 7,
    OakLog = 8,
    OakLeaves = 9,
    Count, // This should always be the last element, used to count the number of block types
};

inline bool hasTint(BlockId blockId) {
    return blockId == BlockId::Grass || blockId == BlockId::Dirt;
}
