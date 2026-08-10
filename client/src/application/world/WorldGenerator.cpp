#include "WorldGenerator.h"

#include "chunk/Chunk.h"

#include <algorithm>

namespace {

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
    ChunkCoord coord{chunkPosX, chunkPosZ};
    if (const auto it = m_gridCache.find(coord); it != m_gridCache.end()) {
        return it->second;
    }

    thread_local std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = chunkPosZ * Chunk::WIDTH;
    const int yStart = chunkPosX * Chunk::WIDTH;
    constexpr int xSize = Chunk::WIDTH;
    constexpr int ySize = Chunk::WIDTH;

    m_terrainGenerator->GenUniformGrid2D(floatGrid.data(), xStart, yStart, xSize, ySize, m_noiseFrequency, m_noiseSeed);

    std::vector<uint8_t> uint8Grid(floatGrid.size());
    std::ranges::transform(floatGrid, uint8Grid.begin(), [](const float v) {
        return static_cast<uint8_t>((v + 1.0f) * 127.5f);
    });

    return m_gridCache.emplace(coord, std::move(uint8Grid)).first->second;
}

std::vector<uint8_t> WorldGenerator::generateCaveDensityMap(const int chunkPosX, const int chunkPosY, const int chunkPosZ) const {
    std::vector<float> floatGrid(Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH);
    const int xStart = chunkPosZ * Chunk::WIDTH;
    const int yStart = chunkPosY * Chunk::WIDTH;
    const int zStart = chunkPosX * Chunk::WIDTH;
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
    const int xStart = chunkPosZ * Chunk::WIDTH;
    const int yStart = chunkPosY * Chunk::WIDTH;
    const int zStart = chunkPosX * Chunk::WIDTH;
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

WorldGenerator::Vegetation WorldGenerator::vegetationAt(const int x, const int z) const {
    const uint32_t hash = vegetationHash(x, z, m_noiseSeed);
    if (hash % 180 == 0) {
        return {VegetationType::Tree, static_cast<uint8_t>(4 + (hash >> 8) % 3)};
    }
    if (hash % 70 == 0) {
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
