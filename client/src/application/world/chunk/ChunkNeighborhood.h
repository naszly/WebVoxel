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

    const Chunk* getChunk(int dx, int dy, int dz) const;
};