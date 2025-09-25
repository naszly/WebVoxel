#pragma once

#include "application/common/CompressionUtils.h"
#include "application/domain/VoxelData.h"
#include "common/datastructures/DynamicallyMappedKTree.h"
#include "common/datastructures/Bitmap.h"

template<uint32_t Depth, uint32_t BaseSize>
class BitmappedVoxelTree {
    constexpr static uint32_t SIZE = Utils::pow(BaseSize, Depth);
    using VoxelTreeT = DynamicallyMappedKTree<VoxelData,Depth, BaseSize>;
    using BitmapT = Bitmap<SIZE * SIZE * SIZE>;
public:
    BitmappedVoxelTree() = default;

    BitmappedVoxelTree(const BitmappedVoxelTree& other) {
        m_tree = other.m_tree;
        m_bitmap = other.m_bitmap;
        m_treeDecompressedLength = other.m_treeDecompressedLength;
    }

    BitmappedVoxelTree& operator=(const BitmappedVoxelTree& other) {
        if (this != &other) {
            m_tree = other.m_tree;
            m_bitmap = other.m_bitmap;
            m_treeDecompressedLength = other.m_treeDecompressedLength;
        }
        return *this;
    }

    BitmappedVoxelTree(BitmappedVoxelTree&& other) noexcept
        : m_tree(std::move(other.m_tree)),
          m_bitmap(std::move(other.m_bitmap)),
          m_treeDecompressedLength(other.m_treeDecompressedLength) {}

    BitmappedVoxelTree& operator=(BitmappedVoxelTree&& other) noexcept {
        if (this != &other) {
            m_tree = std::move(other.m_tree);
            m_bitmap = std::move(other.m_bitmap);
            m_treeDecompressedLength = other.m_treeDecompressedLength;
        }
        return *this;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return getTree().getData(x, y, z);
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        uint32_t i = calculateIndex(x, y, z);
        BitmapT& bitmap = getBitmapInternal();
        if (voxel.isEmpty()) {
            bitmap.clear(i);
        } else {
            bitmap.set(i);
        }
        getTree().setData(x, y, z, voxel);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        uint32_t i = calculateIndex(x, y, z);
        return getBitmap().test(i);
    }

    [[nodiscard]] bool isEmpty() const {
        return getBitmap().isEmpty();
    }

    void serialize(std::ostream& os) {
        getTree().serialize(os);
    }

    void deserialize(std::istream& is) {
        getTree().deserialize(is);
        BitmapT bitmap;
        for (uint32_t x = 0; x < SIZE; x++) {
            for (uint32_t y = 0; y < SIZE; y++) {
                for (uint32_t z = 0; z < SIZE; z++) {
                    if (!getVoxel(x, y, z).isEmpty()) {
                        uint32_t i = calculateIndex(x, y, z);
                        bitmap.set(i);
                    }
                }
            }
        }
        m_bitmap = std::move(bitmap);
    }

    void compress() {
        compressTree();
        compressBitmap();
    }

    bool isCompressed() const {
        return isTreeCompressed() && isBitmapCompressed();
    }

    const BitmapT& getBitmap() const {
        return getBitmapInternal();
    }

private:
    mutable std::variant<VoxelTreeT, std::vector<char>> m_tree{};
    mutable std::variant<BitmapT, std::vector<char>> m_bitmap{};

    size_t m_treeDecompressedLength{};

    VoxelTreeT& getTree() const {
        if (std::holds_alternative<VoxelTreeT>(m_tree)) {
            return std::get<VoxelTreeT>(m_tree);
        }

        const auto& blob = std::get<std::vector<char>>(m_tree);
        if (blob.empty()) throw std::runtime_error("Corrupted chunk blob: empty");
        std::vector<char> decompressed = CompressionUtils::decompressData(blob, m_treeDecompressedLength);
        std::istringstream iss(std::string(decompressed.begin(), decompressed.end()));
        VoxelTreeT tree;
        tree.deserialize(iss);
        m_tree = std::move(tree);
        return std::get<VoxelTreeT>(m_tree);
    }

    BitmapT& getBitmapInternal() const {
        if (std::holds_alternative<BitmapT>(m_bitmap)) {
            return std::get<BitmapT>(m_bitmap);
        }

        const auto& blob = std::get<std::vector<char>>(m_bitmap);
        if (blob.empty()) throw std::runtime_error("Corrupted bitmap blob: empty");
        BitmapT bitmap;
        const std::vector<char> decompressed = CompressionUtils::decompressData(blob, bitmap.size());
        memcpy(bitmap.data(), decompressed.data(), bitmap.size());
        m_bitmap = std::move(bitmap);
        return std::get<BitmapT>(m_bitmap);
    }

    void compressTree() {
        if (!std::holds_alternative<VoxelTreeT>(m_tree)) {
            return;
        }

        std::ostringstream oss;
        std::get<VoxelTreeT>(m_tree).serialize(oss);
        m_treeDecompressedLength = static_cast<uint32_t>(oss.str().size());
        auto compressed = CompressionUtils::compressData(oss.str().data(), oss.str().size(), 3);
        m_tree = std::move(compressed);
    }

    void compressBitmap() {
        if (!std::holds_alternative<BitmapT>(m_bitmap)) {
            return;
        }

        BitmapT& bitmap = std::get<BitmapT>(m_bitmap);
        auto compressed = CompressionUtils::compressData(bitmap.data(), bitmap.size(), 3);
        m_bitmap = std::move(compressed);
    }

    bool isTreeCompressed() const {
        return !std::holds_alternative<VoxelTreeT>(m_tree);
    }

    bool isBitmapCompressed() const {
        return !std::holds_alternative<BitmapT>(m_bitmap);
    }

    static uint32_t calculateIndex(const uint32_t x, const uint32_t y, const uint32_t z) {
        return x * SIZE * SIZE + y * SIZE + z;
    }
};