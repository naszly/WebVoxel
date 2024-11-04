#pragma once

#include <cassert>
#include <random>

#include "ChunkData.h"
#include "../WebGPUContext.h"

class Chunk {
public:
    constexpr static int SIZE = 16;

    Chunk() = default;
    ~Chunk() = default;

    void generate(int x, int y, int z) {
        m_Data.fill([x, y, z](int i, int j, int k) {
            VoxelData data{};
            data.r = random() % 255;
            data.g = random() % 255;
            data.b = random() % 255;
            data.a = 255;
            return data;
        });
    }

    void createVertexBuffer(int x, int y, int z);

    [[nodiscard]] const VoxelData& get(const int x, const int y, const int z) const {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        return m_Data(x, y, z);
    }

    [[nodiscard]] WGPUBuffer getVertexBuffer() const {
        return m_VertexBuffer;
    }

    [[nodiscard]] size_t getVertexCount() const {
        return m_VertexCount;
    }

private:
    ChunkData<VoxelData, SIZE, SIZE, SIZE> m_Data;
    WGPUBuffer m_VertexBuffer{};
    size_t m_VertexCount{};
};
