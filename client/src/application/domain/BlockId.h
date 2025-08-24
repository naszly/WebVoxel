#pragma once

#include <cstdint>

enum class BlockId : uint32_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    Sunstone = 4,
    Count, // This should always be the last element, used to count the number of block types
};