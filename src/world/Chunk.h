#pragma once

#include <cassert>
#include <functional>
#include <random>
#include <glm/vec3.hpp>

#include "SparseVoxelTree.h"
#include "../Utils.h"
#include "../WebGPUContext.h"

class Chunk;

struct ChunkNeighbours {
    const Chunk* xMinus{nullptr};
    const Chunk* xPlus{nullptr};
    const Chunk* yMinus{nullptr};
    const Chunk* yPlus{nullptr};
    const Chunk* zMinus{nullptr};
    const Chunk* zPlus{nullptr};

    [[nodiscard]] size_t count() const {
        size_t count = 0;
        if (xMinus) count++;
        if (xPlus) count++;
        if (yMinus) count++;
        if (yPlus) count++;
        if (zMinus) count++;
        if (zPlus) count++;
        return count;
    }
};

struct ChunkVertexBuffer {
    WGPUBuffer buffer{nullptr};
    size_t vertexCount{0};
};

class Chunk {
    static constexpr size_t DEPTH = 6;
    static constexpr size_t NODE_SIZE = 2;
    using SparseVoxelOctTree = SparseVoxelTree<DEPTH, NODE_SIZE>;
public:
    static constexpr size_t SIZE = Utils::pow(NODE_SIZE, DEPTH);
    static_assert(SIZE >= 16 && SIZE <= 256, "SIZE must be between 16 and 256");

    explicit Chunk(const glm::ivec3 position) : m_Position(position) {}
    explicit Chunk(const int x, const int y, const int z) : m_Position(x, y, z) {}
    ~Chunk() {
        deleteVertexBuffer();
    }

    void generate();

    void createVertexBuffer(const ChunkNeighbours& chunkNeighbours);

    void deleteVertexBuffer();

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_Position;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_Data.getVoxel(x, y, z);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_Data.hasVoxel(x, y, z);
    }

    void setVoxel(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_Data.setVoxel(x, y, z, voxel);
        m_Dirty = true;
    }

    [[nodiscard]] ChunkVertexBuffer getVertexBuffer(const std::function<ChunkNeighbours()>& getChunkNeighbours) {
        if (m_Dirty) {
            const auto neighbours = getChunkNeighbours();
            if (neighbours.count() >= 6) {
                createVertexBuffer(neighbours);
                m_Dirty = false;
            }
        }
        return m_VertexBuffer;
    }

    void setDirty() {
        m_Dirty = true;
    }

private:
    glm::ivec3 m_Position{};
    SparseVoxelOctTree m_Data{};
    ChunkVertexBuffer m_VertexBuffer{};
    bool m_Dirty{true};

    static SparseVoxelOctTree::Neighbours getNeighbours(const ChunkNeighbours &chunkNeighbours);
};
