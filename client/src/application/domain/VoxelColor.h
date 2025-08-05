#pragma once

struct VoxelColor {
    uint8_t r, g, b;

    VoxelColor() : r(0), g(0), b(0) {}
    VoxelColor(const uint8_t r, const uint8_t g, const uint8_t b) : r(r), g(g), b(b) {}
    bool operator==(const VoxelColor& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
    bool operator!=(const VoxelColor& other) const {
        return !(*this == other);
    }
};
