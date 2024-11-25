#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

#include "VoxelData.h"
#include "../Log.h"
#include "../Utils.h"
#include "../Timer.h"

namespace internal {
    template<uint32_t DEPTH, uint32_t NODE_SIZE>
    class SparseVoxelTree {
        friend class SparseVoxelTree<DEPTH + 1, NODE_SIZE>;
    public:
        SparseVoxelTree(const SparseVoxelTree&) = delete;
        SparseVoxelTree& operator=(const SparseVoxelTree&) = delete;
        SparseVoxelTree(SparseVoxelTree&&) = delete;
        SparseVoxelTree& operator=(SparseVoxelTree&&) = delete;
    protected:
        SparseVoxelTree() {
            if constexpr (IS_LEAF) {
                std::fill_n(&nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, EMPTY_VOXEL);
            } else {
                std::fill_n(&nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
            }
        }

        ~SparseVoxelTree() {
            if constexpr (!IS_LEAF) {
                for (uint32_t x = 0; x < NODE_SIZE; x++) {
                    for (uint32_t y = 0; y < NODE_SIZE; y++) {
                        for (uint32_t z = 0; z < NODE_SIZE; z++) {
                            if (nodes[x][y][z] != nullptr)
                                delete nodes[x][y][z];
                        }
                    }
                }
            }
        }

        [[nodiscard]] const VoxelData& get(uint32_t x, uint32_t y, uint32_t z) const {
            if constexpr (IS_LEAF) {
                assert(x < NODE_SIZE && y < NODE_SIZE && z < NODE_SIZE);
                [[assume( x < NODE_SIZE && y < NODE_SIZE && z < NODE_SIZE)]];
                return nodes[x][y][z];
            } else {
                const uint32_t nodeX = x / TREE_SIZE;
                const uint32_t nodeY = y / TREE_SIZE;
                const uint32_t nodeZ = z / TREE_SIZE;

                assert(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE);
                [[assume(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE)]];
                const auto& child = nodes[nodeX][nodeY][nodeZ];

                if (child == nullptr) {
                    return EMPTY_VOXEL;
                }

                return child->get(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE);
            }
        }

        void set(uint32_t x, uint32_t y, uint32_t z, const VoxelData &voxel) {
            if constexpr (IS_LEAF) {
                assert(x < NODE_SIZE && y < NODE_SIZE && z < NODE_SIZE);
                [[assume( x < NODE_SIZE && y < NODE_SIZE && z < NODE_SIZE)]];
                nodes[x][y][z] = voxel;
            } else {
                const uint32_t nodeX = x / TREE_SIZE;
                const uint32_t nodeY = y / TREE_SIZE;
                const uint32_t nodeZ = z / TREE_SIZE;

                assert(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE);
                [[assume(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE)]];
                auto& child = nodes[nodeX][nodeY][nodeZ];

                if (child == nullptr) {
                    if (!voxel.isEmpty()) {
                        child = new ChildTree();
                    } else {
                        return;
                    }
                }

                child->set(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE, voxel);
            }
        }

    private:
        static constexpr VoxelData EMPTY_VOXEL{};
        static constexpr bool IS_LEAF = DEPTH == 0;
        static constexpr uint32_t TREE_SIZE = Utils::pow(NODE_SIZE, DEPTH);

        using ChildTree = SparseVoxelTree<DEPTH - 1, NODE_SIZE>;

        using NodeType = std::conditional_t<IS_LEAF, VoxelData, ChildTree*>;

        NodeType nodes[NODE_SIZE][NODE_SIZE][NODE_SIZE];
    };

    template<uint32_t SIZE>
    class BitMap {
    public:
        BitMap() {
            std::fill_n(&data[0], SIZE / sizeof(data_t), 0);
        }

        void set(const uint32_t i) {
            assert(i < SIZE); [[assume(i < SIZE)]];
            data[i / sizeof(data_t)] |= 1 << (i % sizeof(data_t));
        }

        void clear(const uint32_t i) {
            assert(i < SIZE); [[assume(i < SIZE)]];
            data[i / sizeof(data_t)] &= ~(1 << (i % sizeof(data_t)));
        }

        [[nodiscard]] bool test(const uint32_t i) const {
            assert(i < SIZE); [[assume(i < SIZE)]];
            return data[i / sizeof(data_t)] & (1 << (i % sizeof(data_t)));
        }

    private:
        using data_t = uint8_t;
        data_t data[SIZE / sizeof(data_t)]{};
    };
}

// depth: depth of the tree
// size: size of the matrix of nodes
template<uint32_t depth, uint32_t base_size>
class SparseVoxelTree : public internal::SparseVoxelTree<depth, base_size> {
    using BaseSparseVoxelTree = internal::SparseVoxelTree<depth, base_size>;
    constexpr static uint32_t size = Utils::pow(base_size, depth);
public:

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return BaseSparseVoxelTree::get(x, y, z);
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        BaseSparseVoxelTree::set(x, y, z, voxel);
        uint32_t i = calculateIndex(x+1, y+1, z+1, bitmap_size);
        if (!voxel.isEmpty()) {
            m_bitmap.set(i);
        } else {
            m_bitmap.clear(i);
        }
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        uint32_t i = calculateIndex(x+1, y+1, z+1, bitmap_size);
        return m_bitmap.test(i);
    }

    struct Neighbours {
        const SparseVoxelTree* xMinus{nullptr};
        const SparseVoxelTree* xPlus{nullptr};
        const SparseVoxelTree* yMinus{nullptr};
        const SparseVoxelTree* yPlus{nullptr};
        const SparseVoxelTree* zMinus{nullptr};
        const SparseVoxelTree* zPlus{nullptr};
    };

    [[nodiscard]] std::vector<VertexData> getVertices(const Neighbours &neighbours) const {
        BitMap bitmap = m_bitmap;

        for (uint32_t y = 1; y < bitmap_size - 1; y++) {
            for (uint32_t z = 1; z < bitmap_size - 1; z++) {
                if (neighbours.xPlus == nullptr || neighbours.xPlus->hasVoxel(0, y-1, z-1)) {
                    const uint32_t i = calculateIndex(bitmap_size-1, y, z, bitmap_size);
                    bitmap.set(i);
                }
                if (neighbours.xMinus == nullptr || neighbours.xMinus->hasVoxel(size-1, y-1, z-1)) {
                    const uint32_t i = calculateIndex(0, y, z, bitmap_size);
                    bitmap.set(i);
                }
            }
        }

        for (uint32_t x = 1; x < bitmap_size - 1; x++) {
            for (uint32_t z = 1; z < bitmap_size - 1; z++) {
                if (neighbours.yPlus == nullptr || neighbours.yPlus->hasVoxel(x-1, 0, z-1)) {
                    const uint32_t i = calculateIndex(x, bitmap_size-1, z, bitmap_size);
                    bitmap.set(i);
                }
                if (neighbours.yMinus == nullptr || neighbours.yMinus->hasVoxel(x-1, size-1, z-1)) {
                    const uint32_t i = calculateIndex(x, 0, z, bitmap_size);
                    bitmap.set(i);
                }
            }
        }

        for (uint32_t x = 1; x < bitmap_size - 1; x++) {
            for (uint32_t y = 1; y < bitmap_size - 1; y++) {
                if (neighbours.zPlus == nullptr || neighbours.zPlus->hasVoxel(x-1, y-1, 0)) {
                    const uint32_t i = calculateIndex(x, y, bitmap_size-1, bitmap_size);
                    bitmap.set(i);
                }
                if (neighbours.zMinus == nullptr || neighbours.zMinus->hasVoxel(x-1, y-1, size-1)) {
                    const uint32_t i = calculateIndex(x, y, 0, bitmap_size);
                    bitmap.set(i);
                }
            }
        }

        return getVertices(bitmap);
    }

private:
    static constexpr uint32_t bitmap_size = size + 2;
    using BitMap = internal::BitMap<bitmap_size * bitmap_size * bitmap_size>;
    BitMap m_bitmap{};

    static uint32_t calculateIndex(const uint32_t x, const uint32_t y, const uint32_t z, const uint32_t bitmap_size) {
        return x * bitmap_size * bitmap_size + y * bitmap_size + z;
    }

    [[nodiscard]] std::vector<VertexData> getVertices(const BitMap& bitmap) const {
        std::vector<VertexData> vertices;

        for (uint32_t x = 1; x < bitmap_size - 1; x++) {
            for (uint32_t y = 1; y < bitmap_size - 1; y++) {
                for (uint32_t z = 1; z < bitmap_size - 1; z++) {
                    const uint32_t i = calculateIndex(x, y, z, bitmap_size);

                    if (!bitmap.test(i)) {
                        continue;
                    }

                    const bool isVisible =
                        !(bitmap.test(i-1) && bitmap.test(i+1) &&
                          bitmap.test(i-bitmap_size) && bitmap.test(i+bitmap_size) &&
                          bitmap.test(i-bitmap_size*bitmap_size) && bitmap.test(i+bitmap_size*bitmap_size));

                    if (isVisible) {
                        const auto& voxel = getVoxel(x-1, y-1, z-1);
                        vertices.emplace_back(x-1, y-1, z-1, 1, voxel);
                    }
                }
            }
        }

        return vertices;
    }
};