#pragma once

#include <array>

class Chunk;

class ChunkNeighborhood {
public:
    std::array<std::array<std::array<const Chunk*, 3>, 3>, 3> neighborhood{};

    explicit ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood)
        : neighborhood(neighborhood) {}

    [[nodiscard]] bool hasAllNeighbours() const;
    [[nodiscard]] bool anyNeighbourDirty() const;

    const Chunk* getChunk(int x, int y, int z) const;
    const Chunk* getCenterChunk() const { return neighborhood[1][1][1]; }
};