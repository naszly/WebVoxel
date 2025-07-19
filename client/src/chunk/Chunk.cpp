#include "../chunk/Chunk.h"

#include "../Application.h"
#include "../common/Timer.h"
#include "../common/Log.h"
#include "../common/FileSystem.h"

void Chunk::generate(const FastNoise::SmartNode<> &fnGenerator) {
    const Timer timer("Chunk::generate");

    if (m_Position.y >= 0 && m_Position.y * WIDTH < 256) {
        std::vector<float> noise(WIDTH * WIDTH);

        const size_t xStart = m_Position.z * WIDTH;
        const size_t yStart = m_Position.x * WIDTH;
        fnGenerator->GenUniformGrid2D(noise.data(), xStart, yStart, WIDTH, WIDTH, 0.0004f, 0);

        for (int i = 0; i < WIDTH; i++) {
            for (int j = 0; j < WIDTH; j++) {
                for (int k = 0; k < WIDTH; k++) {
                    const int noiseValue = static_cast<int>((noise[i * WIDTH + k] * 0.5 + 0.5) * 255);
                    const int height = m_Position.y * WIDTH + j;
                    if (noiseValue == height) {
                        m_Data.setVoxel(i, j, k, VoxelData(0, 160 + (random() % 64), 0));
                    } else if (noiseValue > height) {
                        m_Data.setVoxel(i, j, k, VoxelData(135 + (random() % 20 - 10), 69 + (random() % 20 - 10), 19 + (random() % 20 - 10)));
                    }
                }
            }
        }
    } else if (m_Position.y < 0) {
        for (int i = 0; i < WIDTH; i++) {
            for (int j = 0; j < WIDTH; j++) {
                for (int k = 0; k < WIDTH; k++) {
                    m_Data.setVoxel(i, j, k, VoxelData(66 + (random() % 8), 67 + (random() % 8), 69 + (random() % 10)));
                }
            }
        }
    }
}

bool Chunk::fileExists() const {
    const std::string &fileName = getFileName();

    return FileSystem::FileExists(fileName);
}

void Chunk::save() const {
    const std::string &fileName = getFileName();

    std::vector<char> buffer;
    buffer.resize(m_Data.getSerializedSize() + 1);

    std::ospanstream outStream{buffer};
    m_Data.serialize(outStream);

    FileSystem::WriteFile(fileName, outStream.span().data(), outStream.span().size());
}

void Chunk::load() {
    Timer timer("Chunk::load");

    const std::string &fileName = getFileName();
    const std::vector<char> data = FileSystem::ReadFile(fileName);

    std::ispanstream inStream{data};
    m_Data.deserialize(inStream);

    if (inStream.fail()) {
        LogCore::error("Failed to deserialize chunk data from file: {}", fileName);
    } else {
        LogCore::info("Chunk data loaded from file: {}", fileName);
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
