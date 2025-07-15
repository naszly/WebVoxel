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
                return m_nodes[x][y][z];
            } else {
                const uint32_t nodeX = x / TREE_SIZE;
                const uint32_t nodeY = y / TREE_SIZE;
                const uint32_t nodeZ = z / TREE_SIZE;

                assert(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE);
                [[assume(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE)]];
                const auto& child = m_nodes[nodeX][nodeY][nodeZ];

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
                m_nodes[x][y][z] = voxel;
            } else {
                const uint32_t nodeX = x / TREE_SIZE;
                const uint32_t nodeY = y / TREE_SIZE;
                const uint32_t nodeZ = z / TREE_SIZE;

                assert(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE);
                [[assume(nodeX < NODE_SIZE && nodeY < NODE_SIZE && nodeZ < NODE_SIZE)]];
                auto& child = m_nodes[nodeX][nodeY][nodeZ];

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

        NodeType m_nodes[NODE_SIZE][NODE_SIZE][NODE_SIZE];


        void initialize() {
            if constexpr (IS_LEAF) {
                std::fill_n(&m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, EMPTY_VOXEL);
            } else {
                std::fill_n(&m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
            }
        }

        void clear() {
            if constexpr (!IS_LEAF) {
                for (uint32_t x = 0; x < NODE_SIZE; x++) {
                    for (uint32_t y = 0; y < NODE_SIZE; y++) {
                        for (uint32_t z = 0; z < NODE_SIZE; z++) {
                            if (m_nodes[x][y][z] != nullptr) {
                                delete m_nodes[x][y][z];
                            }
                        }
                    }
                }
            }
        }

        void copyFrom(const SparseVoxelTree& other) {
            if constexpr (!IS_LEAF) {
                std::fill_n(&m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
                for (uint32_t x = 0; x < NODE_SIZE; x++) {
                    for (uint32_t y = 0; y < NODE_SIZE; y++) {
                        for (uint32_t z = 0; z < NODE_SIZE; z++) {
                            if (other.m_nodes[x][y][z] != nullptr) {
                                m_nodes[x][y][z] = new ChildTree(*other.m_nodes[x][y][z]);
                            }
                        }
                    }
                }
            } else {
                std::copy_n(&other.m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &m_nodes[0][0][0]);
            }
        }

        void moveFrom(SparseVoxelTree&& other) {
            if constexpr (!IS_LEAF) {
                std::copy_n(&other.m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &m_nodes[0][0][0]);
                std::fill_n(&other.m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, nullptr);
            } else {
                std::copy_n(&other.m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, &m_nodes[0][0][0]);
                std::fill_n(&other.m_nodes[0][0][0], NODE_SIZE * NODE_SIZE * NODE_SIZE, EMPTY_VOXEL);
            }
        }
    };
}

// depth: depth of the tree
// size: size of the matrix of nodes
template<uint32_t depth, uint32_t base_size>
class SparseVoxelTree : public internal::SparseVoxelTree<depth, base_size> {
    using BaseSparseVoxelTree = internal::SparseVoxelTree<depth, base_size>;
    constexpr static uint32_t SIZE = Utils::pow(base_size, depth);
    static constexpr uint32_t BITMAP_SIZE = SIZE + 2;
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
        uint32_t i = calculateIndex(x+1, y+1, z+1, BITMAP_SIZE);
        if (!voxel.isEmpty()) {
            m_bitmap.set(i);
        } else {
            m_bitmap.clear(i);
        }
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        uint32_t i = calculateIndex(x+1, y+1, z+1, BITMAP_SIZE);
        return m_bitmap.test(i);
    }

    struct Neighbours {
        const SparseVoxelTree& xMinus;
        const SparseVoxelTree& xPlus;
        const SparseVoxelTree& yMinus;
        const SparseVoxelTree& yPlus;
        const SparseVoxelTree& zMinus;
        const SparseVoxelTree& zPlus;
    };

    struct ExtendedNeighbours {
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

        auto nv = neighbours.value();

        auto setBitmapIfNeighbourHasVoxel = [&](const SparseVoxelTree& neighbour, const uint32_t x, const uint32_t y, const uint32_t z) {
            const uint32_t vx = (x-1) % SIZE;
            const uint32_t vy = (y-1) % SIZE;
            const uint32_t vz = (z-1) % SIZE;
            if (neighbour.hasVoxel(vx, vy, vz)) {
                const uint32_t i = calculateIndex(x, y, z, BITMAP_SIZE);
                bitmap.set(i);
            }
        };

        for (uint32_t i = 1; i < BITMAP_SIZE - 1; i++) {
            for (uint32_t j = 1; j < BITMAP_SIZE - 1; j++) {
                setBitmapIfNeighbourHasVoxel(nv.xMinus, 0, i, j);
                setBitmapIfNeighbourHasVoxel(nv.xPlus, BITMAP_SIZE-1, i, j);
                setBitmapIfNeighbourHasVoxel(nv.yMinus, i, 0, j);
                setBitmapIfNeighbourHasVoxel(nv.yPlus, i, BITMAP_SIZE-1, j);
                setBitmapIfNeighbourHasVoxel(nv.zMinus, i, j, 0);
                setBitmapIfNeighbourHasVoxel(nv.zPlus, i, j, BITMAP_SIZE-1);
            }
        }

        return bitmap;
    }

    [[nodiscard]] auto getBitmap(const std::optional<ExtendedNeighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto nv = neighbours.value();

        auto setBitmapIfNeighbourHasVoxel = [&](const SparseVoxelTree& neighbour, const uint32_t x, const uint32_t y, const uint32_t z) {
            const uint32_t vx = (x-1) % SIZE;
            const uint32_t vy = (y-1) % SIZE;
            const uint32_t vz = (z-1) % SIZE;
            if (neighbour.hasVoxel(vx, vy, vz)) {
                const uint32_t i = calculateIndex(x, y, z, BITMAP_SIZE);
                bitmap.set(i);
            }
        };

        for (uint32_t i = 1; i < BITMAP_SIZE - 1; i++) {
            for (uint32_t j = 1; j < BITMAP_SIZE - 1; j++) {
                setBitmapIfNeighbourHasVoxel(nv.xMinus, 0, i, j);
                setBitmapIfNeighbourHasVoxel(nv.xPlus, BITMAP_SIZE-1, i, j);
                setBitmapIfNeighbourHasVoxel(nv.yMinus, i, 0, j);
                setBitmapIfNeighbourHasVoxel(nv.yPlus, i, BITMAP_SIZE-1, j);
                setBitmapIfNeighbourHasVoxel(nv.zMinus, i, j, 0);
                setBitmapIfNeighbourHasVoxel(nv.zPlus, i, j, BITMAP_SIZE-1);
            }

            setBitmapIfNeighbourHasVoxel(nv.xMinusYMinus, 0, 0, i);
            setBitmapIfNeighbourHasVoxel(nv.xMinusYPlus, 0, BITMAP_SIZE-1, i);
            setBitmapIfNeighbourHasVoxel(nv.xPlusYMinus, BITMAP_SIZE-1, 0, i);
            setBitmapIfNeighbourHasVoxel(nv.xPlusYPlus, BITMAP_SIZE-1, BITMAP_SIZE-1, i);
            setBitmapIfNeighbourHasVoxel(nv.xMinusZMinus, 0, i, 0);
            setBitmapIfNeighbourHasVoxel(nv.xMinusZPlus, 0, i, BITMAP_SIZE-1);
            setBitmapIfNeighbourHasVoxel(nv.xPlusZMinus, BITMAP_SIZE-1, i, 0);
            setBitmapIfNeighbourHasVoxel(nv.xPlusZPlus, BITMAP_SIZE-1, i, BITMAP_SIZE-1);
            setBitmapIfNeighbourHasVoxel(nv.yMinusZMinus, i, 0, 0);
            setBitmapIfNeighbourHasVoxel(nv.yMinusZPlus, i, 0, BITMAP_SIZE-1);
            setBitmapIfNeighbourHasVoxel(nv.yPlusZMinus, i, BITMAP_SIZE-1, 0);
            setBitmapIfNeighbourHasVoxel(nv.yPlusZPlus, i, BITMAP_SIZE-1, BITMAP_SIZE-1);
        }

        setBitmapIfNeighbourHasVoxel(nv.xMinusYMinusZMinus, 0, 0, 0);
        setBitmapIfNeighbourHasVoxel(nv.xMinusYMinusZPlus, 0, 0, BITMAP_SIZE-1);
        setBitmapIfNeighbourHasVoxel(nv.xMinusYPlusZMinus, 0, BITMAP_SIZE-1, 0);
        setBitmapIfNeighbourHasVoxel(nv.xMinusYPlusZPlus, 0, BITMAP_SIZE-1, BITMAP_SIZE-1);
        setBitmapIfNeighbourHasVoxel(nv.xPlusYMinusZMinus, BITMAP_SIZE-1, 0, 0);
        setBitmapIfNeighbourHasVoxel(nv.xPlusYMinusZPlus, BITMAP_SIZE-1, 0, BITMAP_SIZE-1);
        setBitmapIfNeighbourHasVoxel(nv.xPlusYPlusZMinus, BITMAP_SIZE-1, BITMAP_SIZE-1, 0);
        setBitmapIfNeighbourHasVoxel(nv.xPlusYPlusZPlus, BITMAP_SIZE-1, BITMAP_SIZE-1, BITMAP_SIZE-1);

        return bitmap;
    }

private:
    Bitmap<BITMAP_SIZE * BITMAP_SIZE * BITMAP_SIZE> m_bitmap{};

    static uint32_t calculateIndex(const uint32_t x, const uint32_t y, const uint32_t z, const uint32_t bitmapSize) {
        return x * bitmapSize * bitmapSize + y * bitmapSize + z;
    }
};