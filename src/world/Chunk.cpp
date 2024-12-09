#include "Chunk.h"

#include "../Application.h"
#include "../Timer.h"

void Chunk::generate() {
    //const Timer timer("Chunk::generate");
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

std::optional<Chunk::SparseVoxelOctTree::Neighbours> Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
    if (!chunkNeighbours.hasAllNeighbours()) {
        return std::nullopt;
    }
    return SparseVoxelOctTree::Neighbours{
        .xPlus = chunkNeighbours.xPlus->m_Data,
        .yMinus = chunkNeighbours.yMinus->m_Data,
        .yPlus = chunkNeighbours.yPlus->m_Data,
        .zMinus = chunkNeighbours.zMinus->m_Data,
        .zPlus = chunkNeighbours.zPlus->m_Data,

        .xMinusYMinus = chunkNeighbours.xMinusYMinus->m_Data,
        .xMinusYPlus = chunkNeighbours.xMinusYPlus->m_Data,
        .xMinusZMinus = chunkNeighbours.xMinusZMinus->m_Data,
        .xMinusZPlus = chunkNeighbours.xMinusZPlus->m_Data,
        .xPlusYMinus = chunkNeighbours.xPlusYMinus->m_Data,
        .xPlusYPlus = chunkNeighbours.xPlusYPlus->m_Data,
        .xPlusZMinus = chunkNeighbours.xPlusZMinus->m_Data,
        .xPlusZPlus = chunkNeighbours.xPlusZPlus->m_Data,
        .yMinusZMinus = chunkNeighbours.yMinusZMinus->m_Data,
        .yMinusZPlus = chunkNeighbours.yMinusZPlus->m_Data,
        .yPlusZMinus = chunkNeighbours.yPlusZMinus->m_Data,
        .yPlusZPlus = chunkNeighbours.yPlusZPlus->m_Data,

        .xMinusYMinusZMinus = chunkNeighbours.xMinusYMinusZMinus->m_Data,
        .xMinusYMinusZPlus = chunkNeighbours.xMinusYMinusZPlus->m_Data,
        .xMinusYPlusZMinus = chunkNeighbours.xMinusYPlusZMinus->m_Data,
        .xMinusYPlusZPlus = chunkNeighbours.xMinusYPlusZPlus->m_Data,
        .xPlusYMinusZMinus = chunkNeighbours.xPlusYMinusZMinus->m_Data,
        .xPlusYMinusZPlus = chunkNeighbours.xPlusYMinusZPlus->m_Data,
        .xPlusYPlusZMinus = chunkNeighbours.xPlusYPlusZMinus->m_Data,
        .xPlusYPlusZPlus = chunkNeighbours.xPlusYPlusZPlus->m_Data,
    };
}