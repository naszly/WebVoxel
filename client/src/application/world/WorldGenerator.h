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
        Forest,
        Hills,
    };

    enum class VegetationType : uint8_t {
        None,
        Tree,
        Bush,
    };

    struct Vegetation {
        VegetationType type;
        uint8_t height;
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
    const FastNoise::SmartNode<> m_terrainGenerator = FastNoise::NewFromEncodedNodeTree("DQAFAAAAAAAAQAgAAAAAAD8AAAAAAA==");
    const FastNoise::SmartNode<> m_biomeGenerator = FastNoise::New<FastNoise::CellularValue>();
    const FastNoise::SmartNode<> m_neighborBiomeGenerator;
    const FastNoise::SmartNode<> m_biomeEdgeGenerator;
    const FastNoise::SmartNode<> m_hillsGenerator = FastNoise::New<FastNoise::OpenSimplex2>();
    const FastNoise::SmartNode<> m_caveGenerator;
    const FastNoise::SmartNode<> m_oreGenerator;

    const float m_noiseFrequency = 0.0004f;
    const float m_biomeFrequency = 0.0025f;
    const float m_hillsFrequency = 0.015f;
    const int m_noiseSeed = 0;
};
