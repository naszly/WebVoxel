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
    ~Chunk() {
        deleteVertexBuffer();
    }

    void generate(int x, int y, int z);

    void createVertexBuffer(int x, int y, int z);

    void deleteVertexBuffer();

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_Position;
    }

    [[nodiscard]] bool isEmpty() const {
        return m_Data.isEmpty();
    }

    [[nodiscard]] const VoxelData& getVoxel(const int x, const int y, const int z) const {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        return m_Data.get(x, y, z);
    }

    void setVoxel(const VoxelData& voxel, const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE);
        assert(y >= 0 && y < SIZE);
        assert(z >= 0 && z < SIZE);

        m_Data.set(x, y, z, voxel);
        m_Dirty = true;
    }

    struct VertexBuffer {
        WGPUBuffer buffer{nullptr};
        size_t vertexCount{0};
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
