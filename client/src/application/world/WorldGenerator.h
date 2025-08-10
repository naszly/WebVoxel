#pragma once

#include <FastNoise/FastNoise.h>
#include <common/datastructures/HashMap.h>
#include <common/Thread.h>
#include <vector>

#include <glm/glm.hpp>

class WorldGenerator {
public:
    WorldGenerator() = default;
    ~WorldGenerator() = default;

    WorldGenerator(const WorldGenerator&) = delete;
    WorldGenerator& operator=(const WorldGenerator&) = delete;
    WorldGenerator(WorldGenerator&&) = delete;
    WorldGenerator& operator=(WorldGenerator&&) = delete;

    std::vector<uint8_t> genUniformGrid2D(int chunkPosX, int chunkPosZ);

    void pruneCacheByDistance(const glm::ivec3& currentPosition, int distance);

private:
    using ChunkCoord = std::pair<int, int>;
    HashMap<ChunkCoord, std::vector<uint8_t>> m_gridCache;
    Threading::Lock m_cacheLock;
    const FastNoise::SmartNode<> m_fnGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");
    const float m_noiseFrequency = 0.0004f;
    const int m_noiseSeed = 0;
};
