#include "Chunk.h"
#include "TreeGenerator.h"

#include "application/common/CompressionUtils.h"
#include "common/Log.h"
#include "common/FileSystem.h"

#include <array>

namespace {

constexpr int MaxVegetationRadius = 2;
constexpr int MaxVegetationHeight = 7;
constexpr int ChunkWidth = static_cast<int>(Chunk::WIDTH);

}

static uint8_t hashXz(const int x, const int z) {
    uint32_t v = static_cast<uint32_t>(x) * 0x27d4eb2d ^ static_cast<uint32_t>(z) * 0x85ebca6b;
    v ^= v >> 15;
    return static_cast<uint8_t>(v);
}

static uint32_t hash3(const int x, const int y, const int z){
    uint32_t h = static_cast<uint32_t>(x) * 0x8da6b343u ^ static_cast<uint32_t>(z) * 0xd8163841u ^ static_cast<uint32_t>(y) * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x9e3779b1u; h ^= h >> 15;
    return h;
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

static BlockId maybeOre(const BlockId base, const uint8_t oreNoise,
                        const int gx, const int gy, const int gz) {
    if (base != BlockId::Blackrock) return base;

    if (oreNoise >= 225) {
        return BlockId::EclipseCrystal;
    }
    if (oreNoise >= 210) {
        if ((hash3(gx,gy,gz) & 0xFF) > 180) {
            return BlockId::EclipseCrystal;
        }
    }
    return base;
}

void Chunk::generate(WorldGenerator& generator) {
    thread_local std::vector<uint8_t> terrainHeightMap;
    thread_local std::vector<uint8_t> caveDensityMap;
    thread_local std::vector<uint8_t> oreDensityMap;

    const bool isSurfaceChunk = m_position.y >= 0 && m_position.y * ChunkWidth < 256;
    const bool canContainVegetation = m_position.y >= 0 && m_position.y * ChunkWidth < 256 + MaxVegetationHeight;
    const bool isUndergroundChunk = m_position.y < 0;

    if (isSurfaceChunk || isUndergroundChunk) {
        terrainHeightMap = generator.generateTerrainHeights(m_position.x, m_position.z);
        caveDensityMap = generator.generateCaveDensityMap(m_position.x, m_position.y, m_position.z);
        oreDensityMap = generator.generateOreDensityMap(m_position.x, m_position.y, m_position.z);
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
                            // Grass tint will be applied later with biome info
                            setVoxelInternal(VoxelData(BlockId::Grass), i, j, k);
                        } else if (height >= noiseValue - 2) {
                            setVoxelInternal(VoxelData(BlockId::Dirt), i, j, k);
                        } else {
                            setVoxelInternal(VoxelData(layeredStone(noiseValue, height,
                                                       m_position.x * WIDTH + i,
                                                       m_position.z * WIDTH + k)), i, j, k);
                        }
                    }
                } else if (isUndergroundChunk) {
                    if (!isCaveAt(i, j, k)) {
                        BlockId b = layeredStone(noiseValue, height,
                                               m_position.x * WIDTH + i,
                                               m_position.z * WIDTH + k);
                        const int oreIdx = i * WIDTH * WIDTH + j * WIDTH + k;
                        b = maybeOre(b, oreDensityMap[oreIdx],
                                     m_position.x * WIDTH + i,
                                     height,
                                     m_position.z * WIDTH + k);

                        setVoxelInternal(VoxelData(b), i, j, k);
                    }
                }
            }
        }
    }

    if (!canContainVegetation) return;

    std::array<std::array<std::vector<uint8_t>, 3>, 3> neighboringHeightMaps;
    std::array<std::array<std::vector<WorldGenerator::BiomeType>, 3>, 3> neighboringBiomeMaps;
    for (int chunkOffsetX = -1; chunkOffsetX <= 1; ++chunkOffsetX) {
        for (int chunkOffsetZ = -1; chunkOffsetZ <= 1; ++chunkOffsetZ) {
            neighboringHeightMaps[chunkOffsetX + 1][chunkOffsetZ + 1] =
                generator.generateTerrainHeights(m_position.x + chunkOffsetX, m_position.z + chunkOffsetZ);
            neighboringBiomeMaps[chunkOffsetX + 1][chunkOffsetZ + 1] =
                generator.generateBiomes(m_position.x + chunkOffsetX, m_position.z + chunkOffsetZ);
        }
    }

    const int chunkStartX = m_position.x * ChunkWidth;
    const int chunkStartY = m_position.y * ChunkWidth;
    const int chunkStartZ = m_position.z * ChunkWidth;
    auto terrainHeightAt = [&](const int globalX, const int globalZ) {
        const int heightChunkX = Utils::divideRoundDown(globalX, ChunkWidth);
        const int heightChunkZ = Utils::divideRoundDown(globalZ, ChunkWidth);
        const auto& heightMap = neighboringHeightMaps
            [heightChunkX - m_position.x + 1]
            [heightChunkZ - m_position.z + 1];
        return static_cast<int>(heightMap[
            Utils::mod(globalX, ChunkWidth) * WIDTH +
            Utils::mod(globalZ, ChunkWidth)]);
    };
    auto biomeAt = [&](const int globalX, const int globalZ) {
        const int biomeChunkX = Utils::divideRoundDown(globalX, ChunkWidth);
        const int biomeChunkZ = Utils::divideRoundDown(globalZ, ChunkWidth);
        const auto& biomeMap = neighboringBiomeMaps
            [biomeChunkX - m_position.x + 1]
            [biomeChunkZ - m_position.z + 1];
        return biomeMap[
            Utils::mod(globalX, ChunkWidth) * WIDTH +
            Utils::mod(globalZ, ChunkWidth)];
    };
    auto setVegetationVoxel = [&](const BlockId block, const int globalX, const int globalY, const int globalZ) {
        const int localX = globalX - chunkStartX;
        const int localY = globalY - chunkStartY;
        const int localZ = globalZ - chunkStartZ;
        if (localX < 0 || localX >= ChunkWidth || localY < 0 || localY >= ChunkWidth ||
            localZ < 0 || localZ >= ChunkWidth) return;
        if (!hasVoxel(localX, localY, localZ) ||
            block == BlockId::OakLog && getVoxel(localX, localY, localZ).getBlockId() == BlockId::OakLeaves) {
            setVoxelInternal(VoxelData(block), localX, localY, localZ);
        }
    };

    // Set the chunk's biome to the biome at the chunk center
    const int centerX = chunkStartX + ChunkWidth / 2;
    const int centerZ = chunkStartZ + ChunkWidth / 2;
    m_biome = biomeAt(centerX, centerZ);

    for (int anchorX = chunkStartX - MaxVegetationRadius;
         anchorX < chunkStartX + ChunkWidth + MaxVegetationRadius; ++anchorX) {
        for (int anchorZ = chunkStartZ - MaxVegetationRadius;
             anchorZ < chunkStartZ + ChunkWidth + MaxVegetationRadius; ++anchorZ) {
            const auto vegetation = generator.vegetationAt(anchorX, anchorZ, biomeAt(anchorX, anchorZ));
            if (vegetation.type == WorldGenerator::VegetationType::None) continue;

            const int surfaceHeight = terrainHeightAt(anchorX, anchorZ);
            if (generator.isCaveAt(anchorX, surfaceHeight, anchorZ)) continue;

            if (vegetation.type == WorldGenerator::VegetationType::Bush) {
                TreeGenerator::generateBush(anchorX, anchorZ, surfaceHeight, setVegetationVoxel);
                continue;
            }

            TreeGenerator::generateTree(anchorX, anchorZ, surfaceHeight, vegetation, setVegetationVoxel);
        }
    }
    
    // Apply grass and dirt tints based on biome
    for (int i = 0; i < WIDTH; i++) {
        for (int k = 0; k < WIDTH; k++) {
            const int globalX = chunkStartX + i;
            const int globalZ = chunkStartZ + k;
            const WorldGenerator::BiomeType biome = biomeAt(globalX, globalZ);
            
            VoxelData::Tint grassTint;
            switch (biome) {
            case WorldGenerator::BiomeType::Forest:
                grassTint = VoxelData::Tint::ForestGrass;
                break;
            case WorldGenerator::BiomeType::BushyPlains:
                grassTint = VoxelData::Tint::BushyPlainsGrass;
                break;
            case WorldGenerator::BiomeType::Plains:
                grassTint = VoxelData::Tint::PlainsGrass;
                break;
            case WorldGenerator::BiomeType::Hills:
                grassTint = VoxelData::Tint::HillsGrass;
                break;
            case WorldGenerator::BiomeType::Mountains:
                grassTint = VoxelData::Tint::MountainsGrass;
                break;
            default:
                grassTint = VoxelData::Tint::None;
                break;
            }
            
            // Apply tint to grass and dirt blocks in this column
            for (int j = WIDTH - 1; j >= 0; j--) {
                const int localX = i;
                const int localY = j;
                const int localZ = k;
                if (hasVoxel(localX, localY, localZ)) {
                    VoxelData voxel = getVoxel(localX, localY, localZ);
                    const BlockId blockId = voxel.getBlockId();
                    if (hasTint(blockId)) {
                        setVoxelInternal(VoxelData(blockId, grassTint), localX, localY, localZ);
                    }
                }
            }
        }
    }
}

bool Chunk::fileExists(const std::string &path) const {
    const std::string fileName = path + '/' + getFileName();

    return FileSystem::fileExists(fileName);
}

void Chunk::save(const std::string &path) {
    const std::string fileName = path + '/' + getFileName();

    const auto fileData = serializeCompressed();
    FileSystem::writeFile(fileName, fileData.data(), fileData.size());
}

std::vector<char> Chunk::serializeCompressed() {
    std::ostringstream oss;
    
    // Write biome as first byte
    const uint8_t biomeValue = static_cast<uint8_t>(m_biome);
    oss.write(reinterpret_cast<const char*>(&biomeValue), sizeof(uint8_t));
    
    m_data.serialize(oss);
    const auto serializedData = oss.str();
    const auto compressedData = CompressionUtils::compressData(serializedData.data(), serializedData.size());

    // First 4 bytes represent the decompressed data length
    std::vector<char> fileData(sizeof(uint32_t) + compressedData.size());
    const uint32_t decompressedLength = static_cast<uint32_t>(serializedData.size());
    std::memcpy(fileData.data(), &decompressedLength, sizeof(uint32_t));
    std::memcpy(fileData.data() + sizeof(uint32_t), compressedData.data(), compressedData.size());

    return fileData;
}

void Chunk::load(const std::string &path) {
    const std::string fileName = path + '/' + getFileName();
    const std::vector<char> compressedData = FileSystem::readFile(fileName);

    loadCompressed(compressedData);
    LogApp::info("Chunk data loaded from file: {}", fileName);
}

void Chunk::loadCompressed(const std::vector<char>& fileData) {
    if (fileData.size() < sizeof(uint32_t)) {
        throw std::runtime_error("Compressed chunk data is missing its length prefix");
    }

    // First 4 bytes represent the decompressed data length
    uint32_t decompressedLength;
    std::memcpy(&decompressedLength, fileData.data(), sizeof(decompressedLength));
    const auto data = CompressionUtils::decompressData(
        std::vector(fileData.begin() + sizeof(uint32_t), fileData.end()),
        decompressedLength
    );

    std::istringstream iss(std::string(data.begin(), data.end()));
    
    // Read biome as first byte
    uint8_t biomeValue;
    iss.read(reinterpret_cast<char*>(&biomeValue), sizeof(uint8_t));
    m_biome = static_cast<WorldGenerator::BiomeType>(biomeValue);
    
    m_data.deserialize(iss);

    if (iss.fail()) {
        throw std::runtime_error("Failed to deserialize compressed chunk data");
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
