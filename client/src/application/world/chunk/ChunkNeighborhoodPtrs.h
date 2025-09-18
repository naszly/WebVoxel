#pragma once

#include <array>
#include <cstdint>

class Chunk;

class ChunkNeighborhoodPtrs {
public:
    std::array<std::array<std::array<const Chunk*, 3>, 3>, 3> neighborhood{};

    explicit ChunkNeighborhoodPtrs(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood)
        : neighborhood(neighborhood) {}

    [[nodiscard]] bool hasAllNeighbours() const;
    [[nodiscard]] bool anyNeighbourDirty() const;

    [[nodiscard]] const Chunk* getChunk(int x, int y, int z) const;
    [[nodiscard]] const Chunk* getCenterChunk() const { return neighborhood[1][1][1]; }
    [[nodiscard]] bool hasVoxelAt(uint32_t x, uint32_t y, uint32_t z) const;
};