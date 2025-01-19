#include "Chunk.h"

#include "../Application.h"
#include "../Timer.h"

void Chunk::generate(const FastNoise::SmartNode<> &fnGenerator) {
    const Timer timer("Chunk::generate");

    if (m_Position.y >= 0) {
        std::vector<float> noise(Chunk::SIZE * Chunk::SIZE);

        const size_t xStart = m_Position.z * Chunk::SIZE;
        const size_t yStart = m_Position.x * Chunk::SIZE;
        fnGenerator->GenUniformGrid2D(noise.data(), xStart, yStart, Chunk::SIZE, Chunk::SIZE, 0.0004f, 0);

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                for (int k = 0; k < SIZE; k++) {
                    const int noiseValue = static_cast<int>((noise[i * SIZE + k] * 0.5 + 0.5) * 512);
                    const int height = m_Position.y * SIZE + j;
                    if (noiseValue == height) {
                        m_Data.setVoxel(i, j, k, VoxelData(0, 160 + (random() % 64), 0));
                    } else if (noiseValue >  height) {
                        m_Data.setVoxel(i, j, k, VoxelData(135 + (random() % 20 - 10), 69 + (random() % 20 - 10), 19 + (random() % 20 - 10)));
                    }
                }
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
