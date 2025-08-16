#include "Chunk.h"

#include "application/Application.h"
#include "application/common/CompressionUtils.h"
#include "common/Log.h"
#include "common/FileSystem.h"

#include <zlib.h>

void Chunk::generate(WorldGenerator& generator) {
    thread_local std::vector<uint8_t> noise;

    const bool isSurfaceChunk = m_position.y >= 0 && m_position.y * WIDTH < 256;
    if (isSurfaceChunk) {
        noise = generator.genUniformGrid2D(m_position.x, m_position.z);
    }

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < WIDTH; j++) {
            for (int k = 0; k < WIDTH; k++) {
                if (isSurfaceChunk) {
                    const int noiseValue = noise[i * WIDTH + k];
                    const int height = m_position.y * WIDTH + j;
                    if (noiseValue == height) {
                        m_data.setVoxel(i, j, k, VoxelData(BlockId::Grass));
                    } else if (noiseValue > height) {
                        m_data.setVoxel(i, j, k, VoxelData(BlockId::Dirt));
                    }
                } else if (m_position.y < 0) {
                    m_data.setVoxel(i, j, k, VoxelData(BlockId::Stone));
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

std::optional<Chunk::SparseVoxelOctTree::Neighbours> Chunk::getNeighbours(const ChukNeighbours &chunkNeighbours) {
    if (!chunkNeighbours.hasAllNeighbours()) {
        return std::nullopt;
    }
    return SparseVoxelOctTree::Neighbours{
        .xMinus = chunkNeighbours.xMinus->m_data,
        .xPlus = chunkNeighbours.xPlus->m_data,
        .yMinus = chunkNeighbours.yMinus->m_data,
        .yPlus = chunkNeighbours.yPlus->m_data,
        .zMinus = chunkNeighbours.zMinus->m_data,
        .zPlus = chunkNeighbours.zPlus->m_data,

        .xMinusYMinus = chunkNeighbours.xMinusYMinus->m_data,
        .xMinusYPlus = chunkNeighbours.xMinusYPlus->m_data,
        .xMinusZMinus = chunkNeighbours.xMinusZMinus->m_data,
        .xMinusZPlus = chunkNeighbours.xMinusZPlus->m_data,
        .xPlusYMinus = chunkNeighbours.xPlusYMinus->m_data,
        .xPlusYPlus = chunkNeighbours.xPlusYPlus->m_data,
        .xPlusZMinus = chunkNeighbours.xPlusZMinus->m_data,
        .xPlusZPlus = chunkNeighbours.xPlusZPlus->m_data,
        .yMinusZMinus = chunkNeighbours.yMinusZMinus->m_data,
        .yMinusZPlus = chunkNeighbours.yMinusZPlus->m_data,
        .yPlusZMinus = chunkNeighbours.yPlusZMinus->m_data,
        .yPlusZPlus = chunkNeighbours.yPlusZPlus->m_data,

        .xMinusYMinusZMinus = chunkNeighbours.xMinusYMinusZMinus->m_data,
        .xMinusYMinusZPlus = chunkNeighbours.xMinusYMinusZPlus->m_data,
        .xMinusYPlusZMinus = chunkNeighbours.xMinusYPlusZMinus->m_data,
        .xMinusYPlusZPlus = chunkNeighbours.xMinusYPlusZPlus->m_data,
        .xPlusYMinusZMinus = chunkNeighbours.xPlusYMinusZMinus->m_data,
        .xPlusYMinusZPlus = chunkNeighbours.xPlusYMinusZPlus->m_data,
        .xPlusYPlusZMinus = chunkNeighbours.xPlusYPlusZMinus->m_data,
        .xPlusYPlusZPlus = chunkNeighbours.xPlusYPlusZPlus->m_data,
    };
}
