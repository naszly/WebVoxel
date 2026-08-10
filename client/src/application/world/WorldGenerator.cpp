#include "WorldGenerator.h"

#include "chunk/Chunk.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr int ChunkWidth = static_cast<int>(Chunk::WIDTH);
constexpr float ForestBoundary = -0.12f;
constexpr float BushyPlainsBoundary = 0.08f;
constexpr float HillsBoundary = 0.25f;

WorldGenerator::BiomeType classifyBiome(const float noise) {
    return noise < ForestBoundary
        ? WorldGenerator::BiomeType::Forest
        : noise < BushyPlainsBoundary
            ? WorldGenerator::BiomeType::Plains
            : noise < HillsBoundary
                ? WorldGenerator::BiomeType::BushyPlains
                : WorldGenerator::BiomeType::Hills;
}

int noiseGridStart(const int chunkPosition) {
    const int64_t start = static_cast<int64_t>(chunkPosition) * ChunkWidth;
    if (start < std::numeric_limits<int>::min() || start > std::numeric_limits<int>::max()) {
        throw std::out_of_range("Chunk coordinate is outside the supported noise range.");
    }
    return static_cast<int>(start);
}

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

FastNoise::SmartNode<> makeBiomeEdgeGenerator() {
    auto generator = FastNoise::New<FastNoise::CellularDistance>();
    generator->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0Sub1);
    return generator;
}

FastNoise::SmartNode<> makeNeighborBiomeGenerator() {
    auto generator = FastNoise::New<FastNoise::CellularValue>();
    generator->SetValueIndex(1);
    return generator;
}

WorldGenerator::WorldGenerator(const WorldGeneratorParams param)
    : m_neighborBiomeGenerator(makeNeighborBiomeGenerator()),
      m_biomeEdgeGenerator(makeBiomeEdgeGenerator()),
      m_caveGenerator(makeCaveGenerator(param.cavesEnabled)),
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

    thread_local std::vector<float> terrainGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> biomeGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> neighborBiomeGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> biomeEdgeGrid(Chunk::WIDTH * Chunk::WIDTH);
    thread_local std::vector<float> hillsGrid(Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = noiseGridStart(coord.second);
    const int yStart = noiseGridStart(coord.first);
    constexpr int xSize = Chunk::WIDTH;
    constexpr int ySize = Chunk::WIDTH;

    m_terrainGenerator->GenUniformGrid2D(
        terrainGrid.data(), xStart, yStart, xSize, ySize, m_noiseFrequency, m_noiseSeed);
    m_biomeGenerator->GenUniformGrid2D(
        biomeGrid.data(), xStart, yStart, xSize, ySize, m_biomeFrequency, m_noiseSeed + 7919);
    m_neighborBiomeGenerator->GenUniformGrid2D(
        neighborBiomeGrid.data(), xStart, yStart, xSize, ySize, m_biomeFrequency, m_noiseSeed + 7919);
    m_biomeEdgeGenerator->GenUniformGrid2D(
        biomeEdgeGrid.data(), xStart, yStart, xSize, ySize, m_biomeFrequency, m_noiseSeed + 7919);
    m_hillsGenerator->GenUniformGrid2D(
        hillsGrid.data(), xStart, yStart, xSize, ySize, m_hillsFrequency, m_noiseSeed + 15401);

    SurfaceData surface{
        .heights = std::vector<uint8_t>(terrainGrid.size()),
        .biomes = std::vector<BiomeType>(biomeGrid.size()),
    };
    for (size_t index = 0; index < terrainGrid.size(); ++index) {
        const float biomeNoise = biomeGrid[index];
        surface.biomes[index] = classifyBiome(biomeNoise);

        float biomeAmplitude = 18.0f;
        float biomeCenter = 122.0f;
        if (surface.biomes[index] == BiomeType::Forest) {
            biomeAmplitude = 36.0f;
            biomeCenter = 126.0f;
        } else if (surface.biomes[index] == BiomeType::Hills) {
            biomeAmplitude = 26.0f;
            biomeCenter = 124.0f;
        }
        const bool neighboringBiomeDiffers = classifyBiome(neighborBiomeGrid[index]) != surface.biomes[index];
        const float interiorBlend = neighboringBiomeDiffers
            ? std::clamp(-biomeEdgeGrid[index] * 3.0f, 0.0f, 1.0f)
            : 1.0f;
        const float amplitude = std::lerp(24.0f, biomeAmplitude, interiorBlend);
        const float center = std::lerp(124.0f, biomeCenter, interiorBlend);
        const float hillHeight = surface.biomes[index] == BiomeType::Hills
            ? (hillsGrid[index] + 1.0f) * 10.0f * interiorBlend
            : 0.0f;
        surface.heights[index] = static_cast<uint8_t>(std::clamp(
            std::lround(center + terrainGrid[index] * amplitude + hillHeight), 0L, 255L));
    }

    m_surfaceCache.emplace(coord, std::move(surface));
}

std::vector<uint8_t> WorldGenerator::generateCaveDensityMap(const int chunkPosX, const int chunkPosY, const int chunkPosZ) const {
    std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = noiseGridStart(chunkPosZ);
    const int yStart = noiseGridStart(chunkPosY);
    const int zStart = noiseGridStart(chunkPosX);
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
    const int xStart = noiseGridStart(chunkPosZ);
    const int yStart = noiseGridStart(chunkPosY);
    const int zStart = noiseGridStart(chunkPosX);
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
    if (biome == BiomeType::Forest && hash % 55 == 0) {
        return {VegetationType::Tree, static_cast<uint8_t>(4 + (hash >> 8) % 3)};
    }
    if ((biome == BiomeType::Forest && hash % 35 == 0) ||
        (biome == BiomeType::BushyPlains && hash % 45 == 0)) {
        return {VegetationType::Bush, 1};
    }
    return {VegetationType::None, 0};
}

bool WorldGenerator::isCaveAt(const int x, const int y, const int z) const {
    constexpr float frequency = 0.005f;
    constexpr float caveThreshold = 125.0f / 127.5f - 1.0f;
    return m_caveGenerator->GenSingle3D(
        z * frequency, y * frequency, x * frequency, m_noiseSeed) > caveThreshold;
}

void WorldGenerator::pruneCacheByDistance(const glm::ivec3& currentChunkPosition, const int distance) {
    Threading::ScopedLock lock(&m_cacheLock);
    const int64_t distanceSquared = static_cast<int64_t>(distance) * distance;

    for (auto it = m_surfaceCache.begin(); it != m_surfaceCache.end();) {
        const int64_t x = static_cast<int64_t>(it->first.first) - currentChunkPosition.x;
        const int64_t z = static_cast<int64_t>(it->first.second) - currentChunkPosition.z;
        if (x * x + z * z > distanceSquared) {
            m_surfaceCache.erase(it++);
        } else {
            ++it;
        }
    }
}
