#pragma once

#include <cassert>
#include <random>

#include "SparseVoxelTree.h"
#include "../Utils.h"
#include "../WebGPUContext.h"

class Chunk {
    static constexpr size_t DEPTH = 6;
    static constexpr size_t NODE_SIZE = 2;
public:
    static constexpr size_t SIZE = Utils::pow(NODE_SIZE, DEPTH);

    Chunk() = default;
    ~Chunk() = default;

    void generate(int x, int y, int z) {
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                for (int k = 0; k < SIZE; k++) {

                    VoxelData data{};
                    data.r = random() % 255;
                    data.g = random() % 255;
                    data.b = random() % 255;
                    data.a = 255;

                    m_Data.set(i, j, k, data);

                }
            }
        }
    }

    void createVertexBuffer(int x, int y, int z);

    [[nodiscard]] const VoxelData& get(const int x, const int y, const int z) const {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        return m_Data.get(x, y, z);
    }

    [[nodiscard]] WGPUBuffer getVertexBuffer() const {
        return m_VertexBuffer;
    }

    [[nodiscard]] size_t getVertexCount() const {
        return m_VertexCount;
    }

private:
    SparseVoxelTree<DEPTH, NODE_SIZE> m_Data{};
    WGPUBuffer m_VertexBuffer{};
    size_t m_VertexCount{};
};
