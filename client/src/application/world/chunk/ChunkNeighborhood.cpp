#include "ChunkNeighborhood.h"

#include "Chunk.h"

bool ChunkNeighborhood::hasAllNeighbours() const {
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                if (!neighborhood[x][y][z]) return false;
            }
        }
    }
    return true;
}

bool ChunkNeighborhood::anyNeighbourDirty() const {
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                if (neighborhood[x][y][z] && neighborhood[x][y][z]->isGpuBufferDirty()) {
                    return true;
                }
            }
        }
    }
    return false;
}

const Chunk* ChunkNeighborhood::getChunk(const int x, const int y, const int z) const {
    assert(x < 3 && y < 3 && z < 3);
    return neighborhood[x][y][z];
}
