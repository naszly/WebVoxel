#include "Chunk.h"

#include "../Application.h"
#include "../Timer.h"

void Chunk::generate() {
    const Timer timer("Chunk::generate");
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            for (int k = 0; k < SIZE; k++) {
                const uint32_t randomValue = random();
                VoxelData voxel(
                    randomValue & 0xFF,
                    (randomValue >> 8) & 0xFF,
                    (randomValue >> 16) & 0xFF
                );
                m_Data.setVoxel(i, j, k, voxel);
            }
        }
    }
}

Chunk::SparseVoxelOctTree::Neighbours Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
    return {
        .xMinus = chunkNeighbours.xMinus ? &chunkNeighbours.xMinus->m_Data : nullptr,
        .xPlus = chunkNeighbours.xPlus ? &chunkNeighbours.xPlus->m_Data : nullptr,
        .yMinus = chunkNeighbours.yMinus ? &chunkNeighbours.yMinus->m_Data : nullptr,
        .yPlus = chunkNeighbours.yPlus ? &chunkNeighbours.yPlus->m_Data : nullptr,
        .zMinus = chunkNeighbours.zMinus ? &chunkNeighbours.zMinus->m_Data : nullptr,
        .zPlus = chunkNeighbours.zPlus ? &chunkNeighbours.zPlus->m_Data : nullptr
    };
}
