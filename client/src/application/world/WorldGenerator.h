#pragma once

#include <FastNoise/FastNoise.h>
#include <common/datastructures/HashMap.h>
#include <common/Thread.h>
#include <vector>

#include <glm/glm.hpp>

struct WorldGeneratorParams {
    int seed = 0;
    bool cavesEnabled = true;
};

class WorldGenerator {
public:
    enum class BiomeType : uint8_t {
        Plains,
        BushyPlains,
        OakForest,
        BirchForest,
        Hills,
        Mountains,
        RockyMountains,
    };

    enum class VegetationType : uint8_t {
        None,
        OakTree,
        BirchTree,
        Bush,
    };

    struct Vegetation {
        VegetationType type;
        uint8_t height;
        uint8_t variant;  // 0-3: different oak tree shapes
    };

    explicit WorldGenerator(WorldGeneratorParams param);
    ~WorldGenerator() = default;

    WorldGenerator(const WorldGenerator&) = delete;
    WorldGenerator& operator=(const WorldGenerator&) = delete;
    WorldGenerator(WorldGenerator&&) = delete;
    WorldGenerator& operator=(WorldGenerator&&) = delete;

    std::vector<uint8_t> generateTerrainHeights(int chunkPosX, int chunkPosZ);
    std::vector<BiomeType> generateBiomes(int chunkPosX, int chunkPosZ);

    std::vector<uint8_t> generateCaveDensityMap(int chunkPosX, int chunkPosY, int chunkPosZ) const;

    std::vector<uint8_t> generateOreDensityMap(int chunkPosX, int chunkPosY, int chunkPosZ) const;

    [[nodiscard]] Vegetation vegetationAt(int x, int z, BiomeType biome) const;
    [[nodiscard]] bool isCaveAt(int x, int y, int z) const;

    void pruneCacheByDistance(const glm::ivec3& currentChunkPosition, int distance);

private:
    using ChunkCoord = std::pair<int, int>;
    struct SurfaceData {
        std::vector<uint8_t> heights;
        std::vector<BiomeType> biomes;
    };

    void ensureSurfaceGenerated(const ChunkCoord& coord);

    HashMap<ChunkCoord, SurfaceData> m_surfaceCache;
    Threading::Lock m_cacheLock;
    const FastNoise::SmartNode<> m_baseHeightGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");
    const FastNoise::SmartNode<> m_hillynessGenerator = FastNoise::New<FastNoise::OpenSimplex2>();
    const FastNoise::SmartNode<> m_biomeNoiseGenerator = FastNoise::New<FastNoise::OpenSimplex2>();
    const FastNoise::SmartNode<> m_caveGenerator;
    const FastNoise::SmartNode<> m_oreGenerator;

    const float m_baseHeightFrequency = 0.0004f;
    const float m_hillynessFrequency = 0.008f;  // Lower frequency = bigger hills, fewer of them
    const float m_biomeNoiseFrequency = 0.002f;
    const int m_noiseSeed = 0;
};
