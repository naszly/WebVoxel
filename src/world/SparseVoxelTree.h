#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "VoxelData.h"
#include "Bitmap.h"
#include "../Log.h"
#include "../Utils.h"
#include "../Timer.h"

namespace internal {
    template<uint32_t DEPTH, uint32_t NODE_SIZE>
    class SparseVoxelTree {
        friend class SparseVoxelTree<DEPTH + 1, NODE_SIZE>;
    public:
        SparseVoxelTree(const SparseVoxelTree& other) noexcept {
            copyFrom(other);
        }

        SparseVoxelTree& operator=(const SparseVoxelTree& other) noexcept {
            if (this != &other) {
                clear();
                copyFrom(other);
            }
            return *this;
        }

        SparseVoxelTree(SparseVoxelTree&& other) noexcept {
            moveFrom(std::move(other));
        }

        SparseVoxelTree& operator=(SparseVoxelTree&& other) noexcept {
            if (this != &other) {
                clear();
                moveFrom(std::move(other));
            }
            return *this;
        }

        ~SparseVoxelTree() {
            clear();
        }
    protected:
        SparseVoxelTree() {
            initialize();
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


        void initialize() {
            if constexpr (IS_LEAF) {
                std::fill_n(&nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, EMPTY_VOXEL);
            } else {
                std::fill_n(&nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
            }
        }

        void clear() {
            if constexpr (!IS_LEAF) {
                for (uint32_t x = 0; x < NODE_SIZE; x++) {
                    for (uint32_t y = 0; y < NODE_SIZE; y++) {
                        for (uint32_t z = 0; z < NODE_SIZE; z++) {
                            if (nodes[x][y][z] != nullptr) {
                                delete nodes[x][y][z];
                            }
                        }
                    }
                }
            }
        }

        void copyFrom(const SparseVoxelTree& other) {
            if constexpr (!IS_LEAF) {
                std::fill_n(&nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
                for (uint32_t x = 0; x < NODE_SIZE; x++) {
                    for (uint32_t y = 0; y < NODE_SIZE; y++) {
                        for (uint32_t z = 0; z < NODE_SIZE; z++) {
                            if (other.nodes[x][y][z] != nullptr) {
                                nodes[x][y][z] = new ChildTree(*other.nodes[x][y][z]);
                            }
                        }
                    }
                }
            } else {
                std::copy_n(&other.nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &nodes[0][0][0]);
            }
        }

        void moveFrom(SparseVoxelTree&& other) {
            if constexpr (!IS_LEAF) {
                std::copy_n(&other.nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &nodes[0][0][0]);
                std::fill_n(&other.nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
            } else {
                std::copy_n(&other.nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &nodes[0][0][0]);
                std::fill_n(&other.nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, EMPTY_VOXEL);
            }
        }
    };
}

// depth: depth of the tree
// size: size of the matrix of nodes
template<uint32_t depth, uint32_t base_size>
class SparseVoxelTree : public internal::SparseVoxelTree<depth, base_size> {
    using BaseSparseVoxelTree = internal::SparseVoxelTree<depth, base_size>;
    constexpr static uint32_t size = Utils::pow(base_size, depth);
    static constexpr uint32_t bitmap_size = size + 2;
public:
    SparseVoxelTree() : BaseSparseVoxelTree() {}

    SparseVoxelTree(SparseVoxelTree&& other) noexcept : BaseSparseVoxelTree(std::move(other)) {
        m_bitmap = std::move(other.m_bitmap);
    }

    SparseVoxelTree& operator=(SparseVoxelTree&& other) noexcept {
        if (this != &other) {
            BaseSparseVoxelTree::operator=(std::move(other));
            m_bitmap = std::move(other.m_bitmap);
        }
        return *this;
    }

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
        const SparseVoxelTree& xMinus;
        const SparseVoxelTree& xPlus;
        const SparseVoxelTree& yMinus;
        const SparseVoxelTree& yPlus;
        const SparseVoxelTree& zMinus;
        const SparseVoxelTree& zPlus;

        const SparseVoxelTree& xMinusYMinus;
        const SparseVoxelTree& xMinusYPlus;
        const SparseVoxelTree& xMinusZMinus;
        const SparseVoxelTree& xMinusZPlus;
        const SparseVoxelTree& xPlusYMinus;
        const SparseVoxelTree& xPlusYPlus;
        const SparseVoxelTree& xPlusZMinus;
        const SparseVoxelTree& xPlusZPlus;
        const SparseVoxelTree& yMinusZMinus;
        const SparseVoxelTree& yMinusZPlus;
        const SparseVoxelTree& yPlusZMinus;
        const SparseVoxelTree& yPlusZPlus;

        const SparseVoxelTree& xMinusYMinusZMinus;
        const SparseVoxelTree& xMinusYMinusZPlus;
        const SparseVoxelTree& xMinusYPlusZMinus;
        const SparseVoxelTree& xMinusYPlusZPlus;
        const SparseVoxelTree& xPlusYMinusZMinus;
        const SparseVoxelTree& xPlusYMinusZPlus;
        const SparseVoxelTree& xPlusYPlusZMinus;
        const SparseVoxelTree& xPlusYPlusZPlus;
    };

    [[nodiscard]] auto getBitmap(const std::optional<Neighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto setBitmapIfNeighbourHasVoxel = [&](const SparseVoxelTree& neighbour, const uint32_t x, const uint32_t y, const uint32_t z) {
            const uint32_t vx = (x-1) % size;
            const uint32_t vy = (y-1) % size;
            const uint32_t vz = (z-1) % size;

            if (neighbour.hasVoxel(vx, vy, vz)) {
                const uint32_t i = calculateIndex(x, y, z, bitmap_size);
                bitmap.set(i);
            }
        };

        for (uint32_t i = 1; i < bitmap_size - 1; i++) {
            for (uint32_t j = 1; j < bitmap_size - 1; j++) {
                setBitmapIfNeighbourHasVoxel(neighbours.xMinus, 0, i, j);
                setBitmapIfNeighbourHasVoxel(neighbours.xPlus, bitmap_size-1, i, j);

                setBitmapIfNeighbourHasVoxel(neighbours.yMinus, i, 0, j);
                setBitmapIfNeighbourHasVoxel(neighbours.yPlus, i, bitmap_size-1, j);

                setBitmapIfNeighbourHasVoxel(neighbours.zMinus, i, j, 0);
                setBitmapIfNeighbourHasVoxel(neighbours.zPlus, i, j, bitmap_size-1);
            }

            setBitmapIfNeighbourHasVoxel(neighbours.xMinusYMinus, 0, 0, i);
            setBitmapIfNeighbourHasVoxel(neighbours.xMinusYPlus, 0, bitmap_size-1, i);
            setBitmapIfNeighbourHasVoxel(neighbours.xPlusYMinus, bitmap_size-1, 0, i);
            setBitmapIfNeighbourHasVoxel(neighbours.xPlusYPlus, bitmap_size-1, bitmap_size-1, i);

            setBitmapIfNeighbourHasVoxel(neighbours.xMinusZMinus, 0, i, 0);
            setBitmapIfNeighbourHasVoxel(neighbours.xMinusZPlus, 0, i, bitmap_size-1);
            setBitmapIfNeighbourHasVoxel(neighbours.xPlusZMinus, bitmap_size-1, i, 0);
            setBitmapIfNeighbourHasVoxel(neighbours.xPlusZPlus, bitmap_size-1, i, bitmap_size-1);

            setBitmapIfNeighbourHasVoxel(neighbours.yMinusZMinus, i, 0, 0);
            setBitmapIfNeighbourHasVoxel(neighbours.yMinusZPlus, i, 0, bitmap_size-1);
            setBitmapIfNeighbourHasVoxel(neighbours.yPlusZMinus, i, bitmap_size-1, 0);
            setBitmapIfNeighbourHasVoxel(neighbours.yPlusZPlus, i, bitmap_size-1, bitmap_size-1);
        }

        setBitmapIfNeighbourHasVoxel(neighbours.xMinusYMinusZMinus, 0, 0, 0);
        setBitmapIfNeighbourHasVoxel(neighbours.xMinusYMinusZPlus, 0, 0, bitmap_size-1);
        setBitmapIfNeighbourHasVoxel(neighbours.xMinusYPlusZMinus, 0, bitmap_size-1, 0);
        setBitmapIfNeighbourHasVoxel(neighbours.xMinusYPlusZPlus, 0, bitmap_size-1, bitmap_size-1);
        setBitmapIfNeighbourHasVoxel(neighbours.xPlusYMinusZMinus, bitmap_size-1, 0, 0);
        setBitmapIfNeighbourHasVoxel(neighbours.xPlusYMinusZPlus, bitmap_size-1, 0, bitmap_size-1);
        setBitmapIfNeighbourHasVoxel(neighbours.xPlusYPlusZMinus, bitmap_size-1, bitmap_size-1, 0);
        setBitmapIfNeighbourHasVoxel(neighbours.xPlusYPlusZPlus, bitmap_size-1, bitmap_size-1, bitmap_size-1);

        return bitmap;
    }

private:
    Bitmap<bitmap_size * bitmap_size * bitmap_size> m_bitmap{};

    static uint32_t calculateIndex(const uint32_t x, const uint32_t y, const uint32_t z, const uint32_t bitmap_size) {
        return x * bitmap_size * bitmap_size + y * bitmap_size + z;
    }
};