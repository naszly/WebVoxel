#include "../chunk/Chunk.h"

#include "../Application.h"
#include "../Timer.h"
#include "../Log.h"
#include "../FileSytem.h"

#include <zlib.h>

void Chunk::generate(const FastNoise::SmartNode<> &fnGenerator) {
    const Timer timer("Chunk::generate");

    if (m_Position.y >= 0 && m_Position.y * SIZE < 256) {
        std::vector<float> noise(SIZE * SIZE);

        const size_t xStart = m_Position.z * SIZE;
        const size_t yStart = m_Position.x * SIZE;
        fnGenerator->GenUniformGrid2D(noise.data(), xStart, yStart, SIZE, SIZE, 0.0004f, 0);

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                for (int k = 0; k < SIZE; k++) {
                    const int noiseValue = static_cast<int>((noise[i * SIZE + k] * 0.5 + 0.5) * 255);
                    const int height = m_Position.y * SIZE + j;
                    if (noiseValue == height) {
                        m_Data.setVoxel(i, j, k, VoxelData(0, 160 + (random() % 64), 0));
                    } else if (noiseValue > height) {
                        m_Data.setVoxel(i, j, k, VoxelData(135 + (random() % 20 - 10), 69 + (random() % 20 - 10), 19 + (random() % 20 - 10)));
                    }
                }
            }
        }
    } else if (m_Position.y < 0) {
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                for (int k = 0; k < SIZE; k++) {
                    m_Data.setVoxel(i, j, k, VoxelData(66 + (random() % 8), 67 + (random() % 8), 69 + (random() % 10)));
                }
            }
        }
    }
}

int compress_vector(const std::vector<char> &source, std::vector<char> &destination) {
    const auto sourceData = reinterpret_cast<const Bytef *>(source.data());
    const unsigned long sourceLength = source.size();

    uLongf destinationLength = compressBound(sourceLength);
    destination.resize(destinationLength);
    auto *destinationData = reinterpret_cast<Bytef *>(destination.data());

    const int returnValue = compress2(destinationData, &destinationLength, sourceData, sourceLength, Z_BEST_COMPRESSION);
    if (returnValue == Z_OK) {
        destination.resize(destinationLength);
    } else {
        destination.clear();
    }

    return returnValue;
}

int decompress_vector(const std::vector<char> &source, std::vector<char> &destination) {
    const auto sourceData = reinterpret_cast<const Bytef *>(source.data());
    unsigned long sourceLength = source.size();

    const auto destinationData = reinterpret_cast<Bytef *>(destination.data());
    uLongf destinationLength = destination.size();

    const int returnValue = uncompress2(destinationData, &destinationLength, sourceData, &sourceLength);
    return returnValue;
}

bool Chunk::fileExists() const {
    const std::string &fileName = getFileName();

    return FileSystem::FileExists(fileName);
}

void Chunk::save() const {
    const std::string &fileName = getFileName();

    std::vector<char> data(SIZE * SIZE * SIZE * sizeof(VoxelData));
    const auto voxels = reinterpret_cast<VoxelData*>(data.data());

    for (uint32_t x = 0; x < SIZE; x++) {
        for (uint32_t y = 0; y < SIZE; y++) {
            for (uint32_t z = 0; z < SIZE; z++) {
                const auto index = x * SIZE * SIZE + y * SIZE + z;
                voxels[index] = getVoxel(x, y, z);
            }
        }
    }

    std::vector<char> compressedBuffer;

    compress_vector(data, compressedBuffer);

    FileSystem::WriteFile(fileName, compressedBuffer.data(), compressedBuffer.size());
}

void Chunk::load() {
    Timer timer("Chunk::load");

    const std::string &fileName = getFileName();

    const std::vector<char> compressedData = FileSystem::ReadFile(fileName);

    std::vector<char> data(SIZE * SIZE * SIZE * sizeof(VoxelData));

    decompress_vector(compressedData, data);

    const auto voxels = reinterpret_cast<const VoxelData*>(data.data());

    for (uint32_t x = 0; x < SIZE; x++) {
        for (uint32_t y = 0; y < SIZE; y++) {
            for (uint32_t z = 0; z < SIZE; z++) {
                const auto index = x * SIZE * SIZE + y * SIZE + z;
                setVoxel(voxels[index], x, y, z);
            }
        }
    }
}

void Chunk::CleanFs() {
    FileSystem::CleanFiles(".chunk");
}

std::optional<Chunk::SparseVoxelOctTree::Neighbours> Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
    if (!chunkNeighbours.hasAllNeighbours()) {
        return std::nullopt;
    }
    return SparseVoxelOctTree::Neighbours{
        .xMinus = chunkNeighbours.xMinus->m_Data,
        .xPlus = chunkNeighbours.xPlus->m_Data,
        .yMinus = chunkNeighbours.yMinus->m_Data,
        .yPlus = chunkNeighbours.yPlus->m_Data,
        .zMinus = chunkNeighbours.zMinus->m_Data,
        .zPlus = chunkNeighbours.zPlus->m_Data,
    };
}

std::optional<Chunk::SparseVoxelOctTree::ExtendedNeighbours> Chunk::getNeighbours(const ExtendedChukNeighbours &chunkNeighbours) {
    if (!chunkNeighbours.hasAllNeighbours()) {
        return std::nullopt;
    }
    return SparseVoxelOctTree::ExtendedNeighbours{
        .xMinus = chunkNeighbours.xMinus->m_Data,
        .xPlus = chunkNeighbours.xPlus->m_Data,
        .yMinus = chunkNeighbours.yMinus->m_Data,
        .yPlus = chunkNeighbours.yPlus->m_Data,
        .zMinus = chunkNeighbours.zMinus->m_Data,
        .zPlus = chunkNeighbours.zPlus->m_Data,

        .xMinusYMinus = chunkNeighbours.xMinusYMinus->m_Data,
        .xMinusYPlus = chunkNeighbours.xMinusYPlus->m_Data,
        .xMinusZMinus = chunkNeighbours.xMinusZMinus->m_Data,
        .xMinusZPlus = chunkNeighbours.xMinusZPlus->m_Data,
        .xPlusYMinus = chunkNeighbours.xPlusYMinus->m_Data,
        .xPlusYPlus = chunkNeighbours.xPlusYPlus->m_Data,
        .xPlusZMinus = chunkNeighbours.xPlusZMinus->m_Data,
        .xPlusZPlus = chunkNeighbours.xPlusZPlus->m_Data,
        .yMinusZMinus = chunkNeighbours.yMinusZMinus->m_Data,
        .yMinusZPlus = chunkNeighbours.yMinusZPlus->m_Data,
        .yPlusZMinus = chunkNeighbours.yPlusZMinus->m_Data,
        .yPlusZPlus = chunkNeighbours.yPlusZPlus->m_Data,

        .xMinusYMinusZMinus = chunkNeighbours.xMinusYMinusZMinus->m_Data,
        .xMinusYMinusZPlus = chunkNeighbours.xMinusYMinusZPlus->m_Data,
        .xMinusYPlusZMinus = chunkNeighbours.xMinusYPlusZMinus->m_Data,
        .xMinusYPlusZPlus = chunkNeighbours.xMinusYPlusZPlus->m_Data,
        .xPlusYMinusZMinus = chunkNeighbours.xPlusYMinusZMinus->m_Data,
        .xPlusYMinusZPlus = chunkNeighbours.xPlusYMinusZPlus->m_Data,
        .xPlusYPlusZMinus = chunkNeighbours.xPlusYPlusZMinus->m_Data,
        .xPlusYPlusZPlus = chunkNeighbours.xPlusYPlusZPlus->m_Data,
    };
}
