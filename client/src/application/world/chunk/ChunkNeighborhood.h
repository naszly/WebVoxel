#pragma once

#include <array>

#include "Chunk.h"
#include "ChunkShallow.h"

class ChunkNeighborhood {
public:
    ChunkNeighborhood() = default;
    ChunkNeighborhood(const ChunkNeighborhood&) = delete;
    ChunkNeighborhood& operator=(const ChunkNeighborhood&) = delete;
    ChunkNeighborhood(ChunkNeighborhood&& other) noexcept;
    ChunkNeighborhood& operator=(ChunkNeighborhood&& other) noexcept;

    explicit ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood);

    [[nodiscard]] bool hasAllNeighbours() const;

    [[nodiscard]] const ChunkShallow* getChunk(int x, int y, int z) const;
    [[nodiscard]] const Chunk* getCenterChunk() const;
    [[nodiscard]] bool hasVoxelAt(uint32_t x, uint32_t y, uint32_t z) const;
private:
    static constexpr int SIZE = 3;
    static constexpr int FLAT_SIZE = SIZE * SIZE * SIZE;
    static int index(const int x, const int y, const int z) { return x * SIZE * SIZE + y * SIZE + z; }

    std::array<std::unique_ptr<ChunkShallow>, FLAT_SIZE> m_neighborhood;
    std::unique_ptr<Chunk> m_center;
};