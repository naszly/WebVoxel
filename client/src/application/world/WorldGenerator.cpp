#include "WorldGenerator.h"

#include "chunk/Chunk.h"

#include <algorithm>

std::vector<uint8_t> WorldGenerator::genUniformGrid2D(int chunkPosX, int chunkPosZ) {
    Threading::ScopedLock lock(&m_cacheLock);
    ChunkCoord coord{chunkPosX, chunkPosZ};
    if (const auto it = m_gridCache.find(coord); it != m_gridCache.end()) {
        return it->second;
    }

    thread_local std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = chunkPosZ * Chunk::WIDTH;
    const int yStart = chunkPosX * Chunk::WIDTH;
    constexpr int xSize = Chunk::WIDTH;
    constexpr int ySize = Chunk::WIDTH;

    m_fnGenerator->GenUniformGrid2D(floatGrid.data(), xStart, yStart, xSize, ySize, m_noiseFrequency, m_noiseSeed);

    std::vector<uint8_t> uint8Grid(floatGrid.size());
    std::ranges::transform(floatGrid, uint8Grid.begin(), [](const float v) {
        return static_cast<uint8_t>((v + 1.0f) * 127.5f);
    });

    return m_gridCache.emplace(coord, std::move(uint8Grid)).first->second;
}

void WorldGenerator::pruneCacheByDistance(const glm::ivec3& currentPosition, const int distance) {
    Threading::ScopedLock lock(&m_cacheLock);
    const int distanceSquared = distance * distance;

    for (auto it = m_gridCache.begin(); it != m_gridCache.end();) {
        const int x = it->first.first - currentPosition.z;
        const int y = it->first.second - currentPosition.x;
        if (x * x + y * y > distanceSquared) {
            m_gridCache.erase(it++);
        } else {
            ++it;
        }
    }
}
