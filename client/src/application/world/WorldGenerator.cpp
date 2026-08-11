#include "WorldGenerator.h"

#include "chunk/Chunk.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int ChunkWidth = static_cast<int>(Chunk::WIDTH);

uint32_t vegetationHash(const int x, const int z, const int seed) {
    uint32_t hash = static_cast<uint32_t>(seed) + 0x9e3779b9u;
    hash ^= static_cast<uint32_t>(x) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    hash ^= static_cast<uint32_t>(z) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

}

FastNoise::SmartNode<> makeCaveGenerator(const bool cavesEnabled) {
    if (cavesEnabled) {
        return FastNoise::NewFromEncodedNodeTree("EwCamZk+GgABEQACAAAAAADgQBAAAACIQR8AFgABAAAACwADAAAAAgAAAAMAAAAEAAAAAAAAAD8BFAD//wAAAAAAAD8AAAAAPwAAAAA/AAAAAD8BFwAAAIC/AACAPz0KF0BSuB5AEwAAAKBABgAAj8J1PACamZk+AAAAAAAA4XoUPw==EwCamZk+GgABEQACAAAAAADgQBAAAACIQR8AFgABAAAACwADAAAAAgAAAAMAAAAEAAAAAAAAAD8BFAD//wAAAAAAAD8AAAAAPwAAAAA/AAAAAD8BFwAAAIC/AACAPz0KF0BSuB5AEwAAAKBABgAAj8J1PACamZk+AAAAAAAA4XoUPw==");
    }
    auto constant = FastNoise::New<FastNoise::Constant>();
    constant->SetValue(-1.0f);
    return constant;
}

FastNoise::SmartNode<> makeOreGenerator(const bool cavesEnabled) {
    if (cavesEnabled) {
        return FastNoise::New<FastNoise::OpenSimplex2>();
    }
    auto constant = FastNoise::New<FastNoise::Constant>();
    constant->SetValue(-1.0f);
    return constant;
}

WorldGenerator::WorldGenerator(const WorldGeneratorParams param)
    : m_caveGenerator(makeCaveGenerator(param.cavesEnabled)),
      m_oreGenerator(makeOreGenerator(param.cavesEnabled)),
      m_noiseSeed(param.seed) {}

std::vector<uint8_t> WorldGenerator::generateTerrainHeights(int chunkPosX, int chunkPosZ) {
    Threading::ScopedLock lock(&m_cacheLock);
    const ChunkCoord coord{chunkPosX, chunkPosZ};
    ensureSurfaceGenerated(coord);
    return m_surfaceCache.at(coord).heights;
}

std::vector<WorldGenerator::BiomeType> WorldGenerator::generateBiomes(const int chunkPosX, const int chunkPosZ) {
    Threading::ScopedLock lock(&m_cacheLock);
    const ChunkCoord coord{chunkPosX, chunkPosZ};
    ensureSurfaceGenerated(coord);
    return m_surfaceCache.at(coord).biomes;
}

void WorldGenerator::ensureSurfaceGenerated(const ChunkCoord& coord) {
    if (m_surfaceCache.contains(coord)) return;

    thread_local std::vector<float> baseHeightGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> hillynessGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> biomeNoiseGrid(Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = coord.second * ChunkWidth;
    const int yStart = coord.first * ChunkWidth;
    constexpr int xSize = Chunk::WIDTH;
    constexpr int ySize = Chunk::WIDTH;

    m_baseHeightGenerator->GenUniformGrid2D(
        baseHeightGrid.data(), xStart, yStart, xSize, ySize, m_baseHeightFrequency, m_noiseSeed);
    m_hillynessGenerator->GenUniformGrid2D(
        hillynessGrid.data(), xStart, yStart, xSize, ySize, m_hillynessFrequency, m_noiseSeed + 1);
    m_biomeNoiseGenerator->GenUniformGrid2D(
        biomeNoiseGrid.data(), xStart, yStart, xSize, ySize, m_biomeNoiseFrequency, m_noiseSeed + 2);

    SurfaceData surface{
        .heights = std::vector<uint8_t>(baseHeightGrid.size()),
        .biomes = std::vector<BiomeType>(baseHeightGrid.size()),
    };
    for (size_t index = 0; index < baseHeightGrid.size(); ++index) {
        // Base height from terrain noise - much larger amplitude to create distinct elevation zones
        const float baseHeight = 128.0f + baseHeightGrid[index] * 80.0f;
        
        // Hilliness varies independently - can make any terrain rougher or flatter
        const float hillynessAmount = std::abs(hillynessGrid[index]) * 6.0f;  // Increased from 4.0 to make hills taller
        const float finalHeight = baseHeight + hillynessAmount;
        
        // Biome classification: from biome noise and height influence
        const float normalizedHeight = std::clamp(finalHeight / 255.0f, 0.0f, 1.0f);
        
        // Height influence: high areas slightly favor forests/mountains, low areas favor plains
        const float heightInfluence = (normalizedHeight - 0.5f) * 0.3f;  // -0.15 to +0.15 contribution
        
        // Biome noise + height creates regions but height doesn't dominate
        const float biomeValue = biomeNoiseGrid[index] + heightInfluence;
        
        // Classify biome: independent of hilliness
        if (biomeValue > 0.55f) {
            surface.biomes[index] = BiomeType::BirchForest;
        } else if (biomeValue > 0.2f) {
            surface.biomes[index] = BiomeType::OakForest;
        } else if (biomeValue > -0.1f) {
            surface.biomes[index] = BiomeType::BushyPlains;
        } else {
            surface.biomes[index] = BiomeType::Plains;
        }
        
        // Hilliness adds detail: can override to Mountains/Hills if very hilly
        if (hillynessAmount > 5.0f && normalizedHeight > 0.65f) {
            surface.biomes[index] = BiomeType::RockyMountains;
        } else if (hillynessAmount > 3.5f && normalizedHeight > 0.50f) {
            surface.biomes[index] = BiomeType::Mountains;
        } else if (hillynessAmount > 2.5f && normalizedHeight > 0.40f) {
            surface.biomes[index] = BiomeType::Hills;
        }
        
        surface.heights[index] = static_cast<uint8_t>(std::clamp(std::lround(finalHeight), 0L, 255L));
    }

    m_surfaceCache.emplace(coord, std::move(surface));
}

std::vector<uint8_t> WorldGenerator::generateCaveDensityMap(const int chunkPosX, const int chunkPosY, const int chunkPosZ) const {
    std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = chunkPosZ * ChunkWidth;
    const int yStart = chunkPosY * ChunkWidth;
    const int zStart = chunkPosX * ChunkWidth;
    constexpr int xSize = Chunk::WIDTH;
    constexpr int ySize = Chunk::WIDTH;
    constexpr int zSize = Chunk::WIDTH;

    m_caveGenerator->GenUniformGrid3D(floatGrid.data(), xStart, yStart, zStart, xSize, ySize, zSize, 0.005, m_noiseSeed);

    std::vector<uint8_t> uint8Grid(floatGrid.size());
    std::ranges::transform(floatGrid, uint8Grid.begin(), [](const float v) {
        return static_cast<uint8_t>((v + 1.0f) * 127.5f);
    });

    return uint8Grid;
}

std::vector<uint8_t> WorldGenerator::generateOreDensityMap(const int chunkPosX, const int chunkPosY, const int chunkPosZ) const {
    std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = chunkPosZ * ChunkWidth;
    const int yStart = chunkPosY * ChunkWidth;
    const int zStart = chunkPosX * ChunkWidth;
    m_oreGenerator->GenUniformGrid3D(floatGrid.data(),
                                     xStart, yStart, zStart,
                                     Chunk::WIDTH, Chunk::WIDTH, Chunk::WIDTH,
                                     0.1f,
                                     m_noiseSeed + 424242);
    std::vector<uint8_t> out(floatGrid.size());
    std::ranges::transform(floatGrid, out.begin(), [](const float v){
        return static_cast<uint8_t>((v + 1.f) * 127.5f);
    });
    return out;
}

WorldGenerator::Vegetation WorldGenerator::vegetationAt(
    const int x, const int z, const BiomeType biome) const {
    const uint32_t hash = vegetationHash(x, z, m_noiseSeed);
    if (biome == BiomeType::OakForest && hash % 55 == 0) {
        // Height 5-11, variant 0-3 for different oak tree shapes
        const uint8_t height = static_cast<uint8_t>(5 + (hash >> 8) % 7);
        const uint8_t variant = (hash >> 12) % 4;  // 0=wide, 1=tall, 2=branching, 3=conical
        return {VegetationType::OakTree, height, variant};
    }
    if (biome == BiomeType::BirchForest && hash % 45 == 0) {
        const uint8_t height = static_cast<uint8_t>(7 + (hash >> 8) % 5);  // 7-11, tall and slim
        return {VegetationType::BirchTree, height, 0};
    }
    if ((biome == BiomeType::OakForest && hash % 35 == 0) ||
        (biome == BiomeType::BushyPlains && hash % 45 == 0)) {
        return {VegetationType::Bush, 1, 0};
    }
    return {VegetationType::None, 0, 0};
}

bool WorldGenerator::isCaveAt(const int x, const int y, const int z) const {
    constexpr float frequency = 0.005f;
    constexpr float caveThreshold = 125.0f / 127.5f - 1.0f;
    return m_caveGenerator->GenSingle3D(
        z * frequency, y * frequency, x * frequency, m_noiseSeed) > caveThreshold;
}

void WorldGenerator::pruneCacheByDistance(const glm::ivec3& currentPosition, const int distance) {
    Threading::ScopedLock lock(&m_cacheLock);
    const int distanceSquared = distance * distance;

    for (auto it = m_surfaceCache.begin(); it != m_surfaceCache.end();) {
        const int x = it->first.first - currentPosition.z;
        const int y = it->first.second - currentPosition.x;
        if (x * x + y * y > distanceSquared) {
            m_surfaceCache.erase(it++);
        } else {
            ++it;
        }
    }
}
