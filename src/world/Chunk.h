#pragma once

#include <cassert>
#include <random>
#include <glm/vec3.hpp>

#include "SparseVoxelTree.h"
#include "../Utils.h"
#include "../WebGPUContext.h"

class Chunk {
    static constexpr size_t DEPTH = 6;
    static constexpr size_t NODE_SIZE = 2;
public:
    static constexpr size_t SIZE = Utils::pow(NODE_SIZE, DEPTH);

    explicit Chunk(const int x, const int y, const int z) : m_Position(x, y, z) {}
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

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_Position;
    }

    [[nodiscard]] const VoxelData& get(const int x, const int y, const int z) const {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        return m_Data.get(x, y, z);
    }

    void set(const VoxelData& voxel, const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        m_Data.set(x, y, z, voxel);
        m_Dirty = true;
    }

    struct VertexBuffer {
        WGPUBuffer buffer;
        size_t vertexCount;
    };

    [[nodiscard]] VertexBuffer getVertexBuffer() {
        if (m_Dirty) {
            createVertexBuffer(m_Position.x, m_Position.y, m_Position.z);
            m_Dirty = false;
        }
        return m_VertexBuffer;
    }

private:
    glm::ivec3 m_Position{};
    SparseVoxelTree<DEPTH, NODE_SIZE> m_Data{};
    VertexBuffer m_VertexBuffer{};
    bool m_Dirty{true};
};
