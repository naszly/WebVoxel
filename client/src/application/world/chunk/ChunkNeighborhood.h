#pragma once

#include <array>

#include "Chunk.h"
#include "ChunkShallow.h"

class ChunkNeighborhood {
public:
    ChunkNeighborhood() = default;
    explicit ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood);

    [[nodiscard]] bool hasAllNeighbours() const;

    [[nodiscard]] const std::optional<ChunkShallow>& getChunk(int x, int y, int z) const;
    [[nodiscard]] const std::optional<Chunk>& getCenterChunk() const { return m_center; }
    [[nodiscard]] bool hasVoxelAt(uint32_t x, uint32_t y, uint32_t z) const;
private:
    std::array<std::array<std::array<std::optional<ChunkShallow>, 3>, 3>, 3> m_neighborhood;
    std::optional<Chunk> m_center;
};