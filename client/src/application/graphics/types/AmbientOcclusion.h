#pragma once

#include <cstdint>

enum class AmbientOcclusion : uint32_t {
    None = 0,
    CornerNxNyNz = 1 << 0,
    CornerNxNyPz = 1 << 1,
    CornerNxPyNz = 1 << 2,
    CornerNxPyPz = 1 << 3,
    CornerPxNyNz = 1 << 4,
    CornerPxNyPz = 1 << 5,
    CornerPxPyNz = 1 << 6,
    CornerPxPyPz = 1 << 7,
    EdgeNxNy = 1 << 8,
    EdgeNxPy = 1 << 9,
    EdgePxNy = 1 << 10,
    EdgePxPy = 1 << 11,
    EdgeNxNz = 1 << 12,
    EdgeNxPz = 1 << 13,
    EdgePxNz = 1 << 14,
    EdgePxPz = 1 << 15,
    EdgeNyNz = 1 << 16,
    EdgeNyPz = 1 << 17,
    EdgePyNz = 1 << 18,
    EdgePyPz = 1 << 19,
};

inline AmbientOcclusion operator|(AmbientOcclusion a, AmbientOcclusion b) {
    return static_cast<AmbientOcclusion>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline AmbientOcclusion& operator|=(AmbientOcclusion& a, const AmbientOcclusion b) {
    a = a | b;
    return a;
}

inline AmbientOcclusion operator&(AmbientOcclusion a, AmbientOcclusion b) {
    return static_cast<AmbientOcclusion>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline AmbientOcclusion& operator&=(AmbientOcclusion& a, const AmbientOcclusion b) {
    a = a & b;
    return a;
}