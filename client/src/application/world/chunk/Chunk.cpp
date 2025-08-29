#include "Chunk.h"

#include "application/Application.h"
#include "application/common/CompressionUtils.h"
#include "common/Log.h"
#include "common/FileSystem.h"


static uint8_t hashXz(const int x, const int z) {
    uint32_t v = static_cast<uint32_t>(x) * 0x27d4eb2d ^ static_cast<uint32_t>(z) * 0x85ebca6b;
    v ^= v >> 15;
    return static_cast<uint8_t>(v);
}

static BlockId layeredStone(const int surfaceH, const int globalY, const int x, const int z) {
    // Depth below surface
    int depth = surfaceH - globalY;
    if (depth < 0) return BlockId::Stone;

    // Jitter boundaries so stripes are not perfectly flat
    const int jitter = hashXz(x, z) % 5 - 2;
    depth += jitter;

    // Stripe thickness
    constexpr int stripe = 31;

    const int band = depth / stripe;
    // Pattern cycle
    switch (band % 7) {
    case 0: return BlockId::Stone;
    case 1: return BlockId::Duskstone;
    case 2: return BlockId::Duskstone;
    case 3: return BlockId::Blackrock;
    case 4: return BlockId::Blackrock;
    case 5: return BlockId::Blackrock;
    case 6: return BlockId::Duskstone;
    default:return BlockId::Stone;
    }
}

void Chunk::generate(WorldGenerator& generator) {
    thread_local std::vector<uint8_t> terrainHeightMap;
    thread_local std::vector<uint8_t> caveDensityMap;

    const bool isSurfaceChunk = m_position.y >= 0 && m_position.y * WIDTH < 256;
    const bool isUndergroundChunk = m_position.y < 0;

    if (isSurfaceChunk || isUndergroundChunk) {
        terrainHeightMap = generator.generateTerrainHeights(m_position.x, m_position.z);
        caveDensityMap = generator.generateCaveDensityMap(m_position.x, m_position.y, m_position.z);
    }

    auto isCaveAt = [&](const int i, const int j, const int k) -> bool {
        const int caveIdx = i * WIDTH * WIDTH + j * WIDTH + k;
        return caveDensityMap.size() > caveIdx && caveDensityMap[caveIdx] > 125;
    };

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < WIDTH; j++) {
            for (int k = 0; k < WIDTH; k++) {
                const int noiseIdx = i * WIDTH + k;
                const int noiseValue = terrainHeightMap[noiseIdx];
                const int height = m_position.y * WIDTH + j;
                if (isSurfaceChunk) {
                    if (height <= noiseValue && !isCaveAt(i, j, k)) {
                        if (height == noiseValue) {
                            m_data.setVoxel(i, j, k, VoxelData(BlockId::Grass));
                        } else if (height >= noiseValue - 2) {
                            m_data.setVoxel(i, j, k, VoxelData(BlockId::Dirt));
                        } else {
                            m_data.setVoxel(i, j, k, VoxelData(layeredStone(noiseValue, height,
                                                          m_position.x * WIDTH + i,
                                                          m_position.z * WIDTH + k)));
                        }
                    }
                } else if (isUndergroundChunk) {
                    if (!isCaveAt(i, j, k)) {
                        m_data.setVoxel(i, j, k, VoxelData(layeredStone(noiseValue, height,
                                                          m_position.x * WIDTH + i,
                                                          m_position.z * WIDTH + k)));
                    }
                }
            }
        }
    }
}

bool Chunk::fileExists() const {
    const std::string &fileName = getFileName();

    return FileSystem::fileExists(fileName);
}

void Chunk::save() {
    const std::string &fileName = getFileName();

    std::ostringstream oss;
    m_data.serialize(oss);
    const auto compressedData = CompressionUtils::compressData(oss.str().data(), oss.str().size());

    // First 4 bytes represent the decompressed data length
    std::vector<char> fileData(sizeof(uint32_t) + compressedData.size());
    const uint32_t decompressedLength = static_cast<uint32_t>(oss.str().size());
    std::memcpy(fileData.data(), &decompressedLength, sizeof(uint32_t));
    std::memcpy(fileData.data() + sizeof(uint32_t), compressedData.data(), compressedData.size());

    FileSystem::writeFile(fileName, fileData.data(), fileData.size());
}

void Chunk::load() {
    const std::string &fileName = getFileName();
    const std::vector<char> compressedData = FileSystem::readFile(fileName);

    // First 4 bytes represent the decompressed data length
    const size_t decompressedLength = *reinterpret_cast<const uint32_t*>(compressedData.data());
    const auto data = CompressionUtils::decompressData(
        std::vector(compressedData.begin() + sizeof(uint32_t), compressedData.end()),
        decompressedLength
    );

    std::istringstream iss(std::string(data.begin(), data.end()));
    m_data.deserialize(iss);

    if (iss.fail()) {
        LogApp::error("Failed to deserialize chunk data from file: {}", fileName);
    } else {
        LogApp::info("Chunk data loaded from file: {}", fileName);
    }

    m_litVoxels.clear();
    for (uint32_t x = 0; x < WIDTH; ++x) {
        for (uint32_t y = 0; y < WIDTH; ++y) {
            for (uint32_t z = 0; z < WIDTH; ++z) {
                if (m_data.hasVoxel(x, y, z)) {
                    const VoxelData& voxel = getVoxel(x, y, z);
                    if (voxel.getBlock().emitsLight()) {
                        m_litVoxels.insert({static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z)});
                    }
                }
            }
        }
    }
}

void Chunk::cleanFs() {
    FileSystem::cleanFiles(".chunk");
}