#include "Chunk.h"

#include "application/Application.h"
#include "application/common/CompressionUtils.h"
#include "common/Log.h"
#include "common/FileSystem.h"

#include <zlib.h>

void Chunk::generate(WorldGenerator& generator) {
    thread_local std::vector<uint8_t> terrainHeightMap;
    thread_local std::vector<uint8_t> caveDensityMap;

    const bool isSurfaceChunk = m_position.y >= 0 && m_position.y * WIDTH < 256;
    const bool isUndergroundChunk = m_position.y < 0;

    if (isSurfaceChunk) {
        terrainHeightMap = generator.generateTerrainHeights(m_position.x, m_position.z);
    }

    if (isSurfaceChunk || isUndergroundChunk) {
        caveDensityMap = generator.generateCaveDensityMap(m_position.x, m_position.y, m_position.z);
    }

    auto isCaveAt = [&](int i, int j, int k) -> bool {
        int caveIdx = i * WIDTH * WIDTH + j * WIDTH + k;
        return caveDensityMap.size() > caveIdx && caveDensityMap[caveIdx] > 125;
    };

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < WIDTH; j++) {
            for (int k = 0; k < WIDTH; k++) {
                if (isSurfaceChunk) {
                    const int noiseIdx = i * WIDTH + k;
                    const int noiseValue = terrainHeightMap[noiseIdx];
                    const int height = m_position.y * WIDTH + j;
                    if (height <= noiseValue && !isCaveAt(i, j, k)) {
                        if (height == noiseValue) {
                            m_data.setVoxel(i, j, k, VoxelData(BlockId::Grass));
                        } else if (height >= noiseValue - 2) {
                            m_data.setVoxel(i, j, k, VoxelData(BlockId::Dirt));
                        } else {
                            m_data.setVoxel(i, j, k, VoxelData(BlockId::Stone));
                        }
                    }
                } else if (isUndergroundChunk) {
                    if (!isCaveAt(i, j, k)) {
                        m_data.setVoxel(i, j, k, VoxelData(BlockId::Stone));
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
}

void Chunk::cleanFs() {
    FileSystem::cleanFiles(".chunk");
}