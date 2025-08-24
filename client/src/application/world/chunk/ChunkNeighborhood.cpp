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

const Chunk* ChunkNeighborhood::getChunk(const int dx, const int dy, const int dz) const {
    const int x = dx + 1, y = dy + 1, z = dz + 1;
    if (x < 3 && y < 3 && z < 3) {
        return neighborhood[x][y][z];
    }
    return nullptr;
}
