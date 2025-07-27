#pragma once

#include <cstdint>

#include "VoxelTree.h"
#include "common/datastructures/Bitmap.h"

template<uint32_t Depth, uint32_t BaseSize>
class BitmappedVoxelTree {
    constexpr static uint32_t SIZE = Utils::pow(BaseSize, Depth);
    static constexpr uint32_t BITMAP_SIZE = SIZE + 2;
    using VoxelTreeT = VoxelTree<Depth, BaseSize>;
    using BitmapT = Bitmap<BITMAP_SIZE * BITMAP_SIZE * BITMAP_SIZE>;

public:
    BitmappedVoxelTree() = default;

    BitmappedVoxelTree(const BitmappedVoxelTree& other) {
        m_tree = other.m_tree;
        m_bitmap = other.m_bitmap;
    }

    BitmappedVoxelTree& operator=(const BitmappedVoxelTree& other) {
        if (this != &other) {
            m_tree = other.m_tree;
            m_bitmap = other.m_bitmap;
        }
        return *this;
    }

    BitmappedVoxelTree(BitmappedVoxelTree&& other) noexcept
        : m_tree(std::move(other.m_tree)), m_bitmap(std::move(other.m_bitmap)) {}

    BitmappedVoxelTree& operator=(BitmappedVoxelTree&& other) noexcept {
        if (this != &other) {
            m_tree = std::move(other.m_tree);
            m_bitmap = std::move(other.m_bitmap);
        }
        return *this;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_tree.getVoxel(x, y, z);
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        uint32_t i = calculateIndex(x+1, y+1, z+1, BITMAP_SIZE);
        if (voxel.isEmpty()) {
            m_bitmap.clear(i);
        } else {
            m_bitmap.set(i);
        }
        m_tree.setVoxel(x, y, z, voxel);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        uint32_t i = calculateIndex(x+1, y+1, z+1, BITMAP_SIZE);
        return m_bitmap.test(i);
    }

    [[nodiscard]] bool isEmpty() const {
        return m_tree.isEmpty();
    }

    void serialize(std::ostream& os) {
        m_tree.serialize(os);
    }

    void deserialize(std::istream& is) {
        m_tree.deserialize(is);
        m_bitmap = Bitmap<BITMAP_SIZE * BITMAP_SIZE * BITMAP_SIZE>{};
        for (uint32_t x = 0; x < SIZE; x++) {
            for (uint32_t y = 0; y < SIZE; y++) {
                for (uint32_t z = 0; z < SIZE; z++) {
                    if (!getVoxel(x, y, z).isEmpty()) {
                        uint32_t i = calculateIndex(x+1, y+1, z+1, BITMAP_SIZE);
                        m_bitmap.set(i);
                    }
                }
            }
        }
    }

    static size_t getMaxSerializedSize() {
        return VoxelTreeT::getMaxSerializedSize();
    }

    [[nodiscard]] auto getBitmap() const {
        return m_bitmap;
    }

    struct Neighbours {
        const BitmappedVoxelTree& xMinus;
        const BitmappedVoxelTree& xPlus;
        const BitmappedVoxelTree& yMinus;
        const BitmappedVoxelTree& yPlus;
        const BitmappedVoxelTree& zMinus;
        const BitmappedVoxelTree& zPlus;
    };

    [[nodiscard]] auto getBitmap(const std::optional<Neighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto neighboursV = neighbours.value();

        for (uint32_t i = 1; i < BITMAP_SIZE - 1; i++) {
            for (uint32_t j = 1; j < BITMAP_SIZE - 1; j++) {
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinus, NEG_SIDE, i, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlus, POS_SIDE, i, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yMinus, i, NEG_SIDE, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yPlus, i, POS_SIDE, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.zMinus, i, j, NEG_SIDE);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.zPlus, i, j, POS_SIDE);
            }
        }

        return bitmap;
    }

    struct ExtendedNeighbours {
        const BitmappedVoxelTree& xMinus;
        const BitmappedVoxelTree& xPlus;
        const BitmappedVoxelTree& yMinus;
        const BitmappedVoxelTree& yPlus;
        const BitmappedVoxelTree& zMinus;
        const BitmappedVoxelTree& zPlus;

        const BitmappedVoxelTree& xMinusYMinus;
        const BitmappedVoxelTree& xMinusYPlus;
        const BitmappedVoxelTree& xMinusZMinus;
        const BitmappedVoxelTree& xMinusZPlus;
        const BitmappedVoxelTree& xPlusYMinus;
        const BitmappedVoxelTree& xPlusYPlus;
        const BitmappedVoxelTree& xPlusZMinus;
        const BitmappedVoxelTree& xPlusZPlus;
        const BitmappedVoxelTree& yMinusZMinus;
        const BitmappedVoxelTree& yMinusZPlus;
        const BitmappedVoxelTree& yPlusZMinus;
        const BitmappedVoxelTree& yPlusZPlus;

        const BitmappedVoxelTree& xMinusYMinusZMinus;
        const BitmappedVoxelTree& xMinusYMinusZPlus;
        const BitmappedVoxelTree& xMinusYPlusZMinus;
        const BitmappedVoxelTree& xMinusYPlusZPlus;
        const BitmappedVoxelTree& xPlusYMinusZMinus;
        const BitmappedVoxelTree& xPlusYMinusZPlus;
        const BitmappedVoxelTree& xPlusYPlusZMinus;
        const BitmappedVoxelTree& xPlusYPlusZPlus;
    };

    [[nodiscard]] auto getBitmap(const std::optional<ExtendedNeighbours> &neighbours) const {
        auto bitmap = m_bitmap;

        if (!neighbours.has_value()) {
            return bitmap;
        }

        auto neighboursV = neighbours.value();

        for (uint32_t i = 1; i < BITMAP_SIZE - 1; i++) {
            for (uint32_t j = 1; j < BITMAP_SIZE - 1; j++) {
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinus, NEG_SIDE, i, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlus, POS_SIDE, i, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yMinus, i, NEG_SIDE, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yPlus, i, POS_SIDE, j);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.zMinus, i, j, NEG_SIDE);
                setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.zPlus, i, j, POS_SIDE);
            }

            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYMinus, NEG_SIDE, NEG_SIDE, i);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYPlus, NEG_SIDE, POS_SIDE, i);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYMinus, POS_SIDE, NEG_SIDE, i);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYPlus, POS_SIDE, POS_SIDE, i);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusZMinus, NEG_SIDE, i, NEG_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusZPlus, NEG_SIDE, i, POS_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusZMinus, POS_SIDE, i, NEG_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusZPlus, POS_SIDE, i, POS_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yMinusZMinus, i, NEG_SIDE, NEG_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yMinusZPlus, i, NEG_SIDE, POS_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yPlusZMinus, i, POS_SIDE, NEG_SIDE);
            setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.yPlusZPlus, i, POS_SIDE, POS_SIDE);
        }

        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYMinusZMinus, NEG_SIDE, NEG_SIDE, NEG_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYMinusZPlus, NEG_SIDE, NEG_SIDE, POS_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYPlusZMinus, NEG_SIDE, POS_SIDE, NEG_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xMinusYPlusZPlus, NEG_SIDE, POS_SIDE, POS_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYMinusZMinus, POS_SIDE, NEG_SIDE, NEG_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYMinusZPlus, POS_SIDE, NEG_SIDE, POS_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYPlusZMinus, POS_SIDE, POS_SIDE, NEG_SIDE);
        setBitmapIfNeighbourHasVoxel(bitmap, neighboursV.xPlusYPlusZPlus, POS_SIDE, POS_SIDE, POS_SIDE);

        return bitmap;
    }

private:
    VoxelTreeT m_tree{};
    BitmapT m_bitmap{};

    static constexpr size_t NEG_SIDE = 0;
    static constexpr size_t POS_SIDE = BITMAP_SIZE - 1;

    static uint32_t calculateIndex(const uint32_t x, const uint32_t y, const uint32_t z, const uint32_t bitmapSize) {
        return x * bitmapSize * bitmapSize + y * bitmapSize + z;
    }

    static void setBitmapIfNeighbourHasVoxel(BitmapT& bitmap, const BitmappedVoxelTree& neighbour,
                                             const uint32_t x, const uint32_t y, const uint32_t z) {
        const uint32_t vx = (x - 1) % SIZE;
        const uint32_t vy = (y - 1) % SIZE;
        const uint32_t vz = (z - 1) % SIZE;
        if (neighbour.hasVoxel(vx, vy, vz)) {
            const uint32_t i = calculateIndex(x, y, z, BITMAP_SIZE);
            bitmap.set(i);
        }
    }
};