#include "Chunk.h"

#include "../Application.h"
#include "../Timer.h"
#include "../FileSytem.h"

#include <zlib.h>

void Chunk::generate(const FastNoise::SmartNode<> &fnGenerator) {
    const Timer timer("Chunk::generate");

    if (m_Position.y >= 0) {
        std::vector<float> noise(Chunk::SIZE * Chunk::SIZE);

        const size_t xStart = m_Position.z * Chunk::SIZE;
        const size_t yStart = m_Position.x * Chunk::SIZE;
        fnGenerator->GenUniformGrid2D(noise.data(), xStart, yStart, Chunk::SIZE, Chunk::SIZE, 0.0004f, 0);

        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                for (int k = 0; k < SIZE; k++) {
                    const int noiseValue = static_cast<int>((noise[i * SIZE + k] * 0.5 + 0.5) * 512);
                    const int height = m_Position.y * SIZE + j;
                    if (noiseValue == height) {
                        m_Data.setVoxel(i, j, k, VoxelData(0, 160 + (random() % 64), 0));
                    } else if (noiseValue >  height) {
                        m_Data.setVoxel(i, j, k, VoxelData(135 + (random() % 20 - 10), 69 + (random() % 20 - 10), 19 + (random() % 20 - 10)));
                    }
                }
            }
        }
    }
}

int compress_vector(const std::vector<char> &source, std::vector<char> &destination) {
    const auto source_data = reinterpret_cast<const Bytef *>(source.data());
    const unsigned long source_length = source.size();

    uLongf destination_length = compressBound(source_length);
    destination.resize(destination_length);
    auto *destination_data = reinterpret_cast<Bytef *>(destination.data());

    const int return_value = compress2(destination_data, &destination_length, source_data, source_length, Z_BEST_COMPRESSION);
    return return_value;
}

int decompress_vector(const std::vector<char> &source, std::vector<char> &destination) {
    const auto source_data = reinterpret_cast<const Bytef *>(source.data());
    unsigned long source_length = source.size();

    const auto destination_data = reinterpret_cast<Bytef *>(destination.data());
    uLongf destination_length = destination.size();

    const int return_value = uncompress2(destination_data, &destination_length, source_data, &source_length);
    return return_value;
}

void Chunk::save(const std::string &fileName) const {
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

void Chunk::load(const std::string &fileName) {
    Timer timer("Chunk::load");

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

Chunk::SparseVoxelOctTree::Neighbours Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
    return {
        .xMinus = chunkNeighbours.xMinus ? &chunkNeighbours.xMinus->m_Data : nullptr,
        .xPlus = chunkNeighbours.xPlus ? &chunkNeighbours.xPlus->m_Data : nullptr,
        .yMinus = chunkNeighbours.yMinus ? &chunkNeighbours.yMinus->m_Data : nullptr,
        .yPlus = chunkNeighbours.yPlus ? &chunkNeighbours.yPlus->m_Data : nullptr,
        .zMinus = chunkNeighbours.zMinus ? &chunkNeighbours.zMinus->m_Data : nullptr,
        .zPlus = chunkNeighbours.zPlus ? &chunkNeighbours.zPlus->m_Data : nullptr
    };
}
