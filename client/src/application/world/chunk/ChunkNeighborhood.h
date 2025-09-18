#pragma once

#include <array>

#include "Chunk.h"

class ChunkNeighborhood {
public:
    std::array<std::array<std::array<std::optional<Chunk>, 3>, 3>, 3> neighborhood;

    ChunkNeighborhood() = default;
    explicit ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood);

    [[nodiscard]] bool hasAllNeighbours() const;
    [[nodiscard]] bool anyNeighbourDirty() const;

    [[nodiscard]] const std::optional<Chunk>& getChunk(int x, int y, int z) const;
    [[nodiscard]] const std::optional<Chunk>& getCenterChunk() const { return neighborhood[1][1][1]; }
    [[nodiscard]] bool hasVoxelAt(uint32_t x, uint32_t y, uint32_t z) const;
};