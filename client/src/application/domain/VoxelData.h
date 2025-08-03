#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

struct VoxelData {
    uint8_t r{0}, g{0}, b{0}, a{0};

    VoxelData() = default;
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b) : r(r), g(g), b(b), a(255) {}
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a) : r(r), g(g), b(b), a(a) {}
    explicit VoxelData(const glm::vec4 color) : r(color.r * 255), g(color.g * 255), b(color.b * 255), a(color.a * 255) {}

    [[nodiscard]] bool isEmpty() const {
        return a == 0;
    }

    bool operator==(const VoxelData& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const VoxelData& other) const {
        return !(*this == other);
    }
};
