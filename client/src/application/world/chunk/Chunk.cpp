#include "Chunk.h"

#include "application/Application.h"
#include "common/Timer.h"
#include "common/Log.h"
#include "common/FileSystem.h"

#include <zlib.h>

void Chunk::generate(const FastNoise::SmartNode<> &fnGenerator) {
    const Timer timer("Chunk::generate");

    std::vector<float> noise(WIDTH * WIDTH);

    const size_t xStart = m_position.z * WIDTH;
    const size_t yStart = m_position.x * WIDTH;
    fnGenerator->GenUniformGrid2D(noise.data(), xStart, yStart, WIDTH, WIDTH, 0.0004f, 0);

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < WIDTH; j++) {
            for (int k = 0; k < WIDTH; k++) {

                if (m_position.y >= 0 && m_position.y * WIDTH < 256) {
                    const int noiseValue = static_cast<int>((noise[i * WIDTH + k] * 0.5 + 0.5) * 255);
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

    const auto compressedData = compressData(oss.str().data(), oss.str().size());

    // First 4 bytes represent the decompressed data length
    std::vector<char> fileData(sizeof(uint32_t) + compressedData.size());
    const uint32_t decompressedLength = static_cast<uint32_t>(oss.str().size());
    std::memcpy(fileData.data(), &decompressedLength, sizeof(uint32_t));
    std::memcpy(fileData.data() + sizeof(uint32_t), compressedData.data(), compressedData.size());

    FileSystem::writeFile(fileName, fileData.data(), fileData.size());
}

void Chunk::load() {
    Timer timer("Chunk::load");

    const std::string &fileName = getFileName();
    const std::vector<char> compressedData = FileSystem::readFile(fileName);

    // First 4 bytes represent the decompressed data length
    const size_t decompressedLength = *reinterpret_cast<const uint32_t*>(compressedData.data());
    const auto data = decompressData(
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

std::vector<char> Chunk::compressData(const void* source, const size_t sourceLength) {
    const auto sourceData = static_cast<const Bytef *>(source);

    uLongf destinationLength = compressBound(sourceLength);
    std::vector<char> destinationBuffer(destinationLength);
    auto *destinationData = reinterpret_cast<Bytef *>(destinationBuffer.data());

    const int result = compress2(destinationData, &destinationLength, sourceData, sourceLength, Z_BEST_COMPRESSION);
    if (result == Z_OK) {
        destinationBuffer.resize(destinationLength);
    } else {
        destinationBuffer.clear();
        LogApp::error("Failed to compress data: {}", zError(result));
    }

    return destinationBuffer;
}

std::vector<char> Chunk::decompressData(const std::vector<char>& source, size_t destinationLength) {
    Timer timer("Chunk::decompressData");
    const auto sourceData = reinterpret_cast<const Bytef *>(source.data());
    unsigned long sourceLength = source.size();

    std::vector<char> destinationBuffer(destinationLength);
    auto *destinationData = reinterpret_cast<Bytef *>(destinationBuffer.data());

    const int result = uncompress2(destinationData, &destinationLength, sourceData, &sourceLength);
    if (result == Z_OK) {
        destinationBuffer.resize(destinationLength);
    } else {
        destinationBuffer.clear();
        LogApp::error("Failed to decompress data: {}", zError(result));
    }

    return destinationBuffer;
}

std::optional<Chunk::SparseVoxelOctTree::Neighbours> Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
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
    };
}

std::optional<Chunk::SparseVoxelOctTree::ExtendedNeighbours> Chunk::getNeighbours(const ExtendedChukNeighbours &chunkNeighbours) {
    if (!chunkNeighbours.hasAllNeighbours()) {
        return std::nullopt;
    }
    return SparseVoxelOctTree::ExtendedNeighbours{
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
