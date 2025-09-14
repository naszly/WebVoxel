#include "AmbientOcclusionComputer.h"

AmbientOcclusion AmbientOcclusionComputer::compute(const ChunkNeighborhood& neighborChunks,
                                                   const uint32_t x, const uint32_t y, const uint32_t z) {
    struct OffsetFlag { int dx, dy, dz; AmbientOcclusion flag; };

    static constexpr OffsetFlag table[] = {
        {-1, -1, -1, AmbientOcclusion::CornerNxNyNz},
        {-1, -1,  1, AmbientOcclusion::CornerNxNyPz},
        {-1,  1, -1, AmbientOcclusion::CornerNxPyNz},
        {-1,  1,  1, AmbientOcclusion::CornerNxPyPz},
        { 1, -1, -1, AmbientOcclusion::CornerPxNyNz},
        { 1, -1,  1, AmbientOcclusion::CornerPxNyPz},
        { 1,  1, -1, AmbientOcclusion::CornerPxPyNz},
        { 1,  1,  1, AmbientOcclusion::CornerPxPyPz},
        {-1, -1,  0, AmbientOcclusion::EdgeNxNy},
        {-1,  1,  0, AmbientOcclusion::EdgeNxPy},
        { 1, -1,  0, AmbientOcclusion::EdgePxNy},
        { 1,  1,  0, AmbientOcclusion::EdgePxPy},
        {-1,  0, -1, AmbientOcclusion::EdgeNxNz},
        {-1,  0,  1, AmbientOcclusion::EdgeNxPz},
        { 1,  0, -1, AmbientOcclusion::EdgePxNz},
        { 1,  0,  1, AmbientOcclusion::EdgePxPz},
        { 0, -1, -1, AmbientOcclusion::EdgeNyNz},
        { 0, -1,  1, AmbientOcclusion::EdgeNyPz},
        { 0,  1, -1, AmbientOcclusion::EdgePyNz},
        { 0,  1,  1, AmbientOcclusion::EdgePyPz},
    };

    auto ao = AmbientOcclusion::None;
    for (const auto& [dx, dy, dz, flag] : table) {
        const uint32_t mask = neighborChunks.hasVoxelAt(x + dx, y + dy, z + dz);
        ao |= static_cast<AmbientOcclusion>(mask * static_cast<uint32_t>(flag));
    }
    return ao;
}

