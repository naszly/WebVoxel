#pragma once

#include <cassert>

#include "VoxelData.h"
#include "Bitmap.h"
#include "KTree.h"
#include "../Utils.h"

// depth: depth of the tree
// size: size of the matrix of nodes
template<uint32_t Depth, uint32_t BaseSize>
class VoxelTree {
    constexpr static uint32_t SIZE = Utils::pow(BaseSize, Depth);
    static constexpr uint32_t BITMAP_SIZE = SIZE + 2;
    KTree<Depth, BaseSize, VoxelData> m_tree{};
public:
    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_tree.get(x, y, z);
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        m_tree.set(x, y, z, voxel);
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
        const VoxelTree& xMinus;
        const VoxelTree& xPlus;
        const VoxelTree& yMinus;
        const VoxelTree& yPlus;
        const VoxelTree& zMinus;
        const VoxelTree& zPlus;
    };

    [[nodiscard]] auto getBitmap(const std::optional<Neighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto nv = neighbours.value();

        auto setBitmapIfNeighbourHasVoxel = [&](const VoxelTree& neighbour, const uint32_t x, const uint32_t y, const uint32_t z) {
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

    struct ExtendedNeighbours {
        const VoxelTree& xMinus;
        const VoxelTree& xPlus;
        const VoxelTree& yMinus;
        const VoxelTree& yPlus;
        const VoxelTree& zMinus;
        const VoxelTree& zPlus;

        const VoxelTree& xMinusYMinus;
        const VoxelTree& xMinusYPlus;
        const VoxelTree& xMinusZMinus;
        const VoxelTree& xMinusZPlus;
        const VoxelTree& xPlusYMinus;
        const VoxelTree& xPlusYPlus;
        const VoxelTree& xPlusZMinus;
        const VoxelTree& xPlusZPlus;
        const VoxelTree& yMinusZMinus;
        const VoxelTree& yMinusZPlus;
        const VoxelTree& yPlusZMinus;
        const VoxelTree& yPlusZPlus;

        const VoxelTree& xMinusYMinusZMinus;
        const VoxelTree& xMinusYMinusZPlus;
        const VoxelTree& xMinusYPlusZMinus;
        const VoxelTree& xMinusYPlusZPlus;
        const VoxelTree& xPlusYMinusZMinus;
        const VoxelTree& xPlusYMinusZPlus;
        const VoxelTree& xPlusYPlusZMinus;
        const VoxelTree& xPlusYPlusZPlus;
    };

    [[nodiscard]] auto getBitmap(const std::optional<ExtendedNeighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto nv = neighbours.value();

        auto setBitmapIfNeighbourHasVoxel = [&](const VoxelTree& neighbour, const uint32_t x, const uint32_t y, const uint32_t z) {
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