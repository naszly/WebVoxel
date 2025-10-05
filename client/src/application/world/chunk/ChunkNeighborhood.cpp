#include "ChunkNeighborhood.h"


ChunkNeighborhood::ChunkNeighborhood(ChunkNeighborhood&& other) noexcept {
    m_neighborhood = std::move(other.m_neighborhood);
    m_center = std::move(other.m_center);

}

ChunkNeighborhood& ChunkNeighborhood::operator=(ChunkNeighborhood&& other) noexcept {
    if (this != &other) {
        m_neighborhood = std::move(other.m_neighborhood);
        m_center = std::move(other.m_center);
    }
    return *this;
}

ChunkNeighborhood::ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood) {
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                int idx = index(x, y, z);
                if (neighborhood[x][y][z] != nullptr) {
                    if (x == 1 && y == 1 && z == 1) {
                        m_center = std::make_unique<Chunk>(*neighborhood[x][y][z]);
                    }
                    m_neighborhood[idx] = std::make_unique<ChunkShallow>(*neighborhood[x][y][z]);
                }
            }
        }
    }
}

bool ChunkNeighborhood::hasAllNeighbours() const {
    return std::ranges::all_of(m_neighborhood, [](const auto& chunk) {
        return chunk != nullptr;
    });
}

const ChunkShallow* ChunkNeighborhood::getChunk(const int x, const int y, const int z) const {
    assert(x < SIZE && y < SIZE && z < SIZE);
    const int idx = index(x, y, z);
    return m_neighborhood[idx] ? m_neighborhood[idx].get() : nullptr;
}

const Chunk* ChunkNeighborhood::getCenterChunk() const {
    return m_center ? m_center.get() : nullptr;
}

bool ChunkNeighborhood::hasVoxelAt(const uint32_t x, const uint32_t y, const uint32_t z) const {
    assert(x < SIZE * Chunk::WIDTH && y < SIZE * Chunk::WIDTH && z < SIZE * Chunk::WIDTH);
    const uint32_t cnx = x / Chunk::WIDTH;
    const uint32_t cny = y / Chunk::WIDTH;
    const uint32_t cnz = z / Chunk::WIDTH;
    const uint32_t bx = x % Chunk::WIDTH;
    const uint32_t by = y % Chunk::WIDTH;
    const uint32_t bz = z % Chunk::WIDTH;
    return getChunk(cnx, cny, cnz)->hasVoxel(bx, by, bz);
}
