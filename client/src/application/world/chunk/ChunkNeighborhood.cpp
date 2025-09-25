#include "ChunkNeighborhood.h"

ChunkNeighborhood::ChunkNeighborhood(const std::array<std::array<std::array<const Chunk*, 3>, 3>, 3>& neighborhood) {
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                if (neighborhood[x][y][z] != nullptr) {
                    if (x == 1 && y == 1 && z == 1) {
                        m_center = *neighborhood[x][y][z];
                    }
                    m_neighborhood[x][y][z] = std::make_optional<ChunkShallow>(*neighborhood[x][y][z]);
                }
            }
        }
    }
}

bool ChunkNeighborhood::hasAllNeighbours() const {
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                if (!m_neighborhood[x][y][z]) return false;
            }
        }
    }
    return true;
}

const std::optional<ChunkShallow>& ChunkNeighborhood::getChunk(const int x, const int y, const int z) const {
    assert(x < 3 && y < 3 && z < 3);
    return m_neighborhood[x][y][z];
}

bool ChunkNeighborhood::hasVoxelAt(const uint32_t x, const uint32_t y, const uint32_t z) const {
    assert(x < 3 * Chunk::WIDTH && y < 3 * Chunk::WIDTH && z < 3 * Chunk::WIDTH);
    const uint32_t cnx = x / Chunk::WIDTH;
    const uint32_t cny = y / Chunk::WIDTH;
    const uint32_t cnz = z / Chunk::WIDTH;
    const uint32_t bx = x % Chunk::WIDTH;
    const uint32_t by = y % Chunk::WIDTH;
    const uint32_t bz = z % Chunk::WIDTH;
    return getChunk(cnx, cny, cnz)->hasVoxel(bx, by, bz);
}
