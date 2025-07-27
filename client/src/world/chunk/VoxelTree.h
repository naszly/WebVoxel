#pragma once

#include "VoxelData.h"
#include "VoxelTreeVariant.h"
#include "common/Utils.h"

template<uint32_t Depth, uint32_t BaseSize>
class VoxelTree {
    using VoxelTreeVariantT = VoxelTreeVariant<Depth, BaseSize>;
    constexpr static uint32_t SIZE = Utils::pow(BaseSize, Depth);
    static constexpr uint32_t BITMAP_SIZE = SIZE + 2;
    using VoxelTree8 = typename VoxelTreeVariantT::template VoxelTreeImpl<VoxelIdSize::U8Bit>;
    using VoxelTree16 = typename VoxelTreeVariantT::template VoxelTreeImpl<VoxelIdSize::U16Bit>;
    using VoxelTree20 = typename VoxelTreeVariantT::template VoxelTreeImpl<VoxelIdSize::U20Bit>;
public:
    VoxelTree() {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.u8Tree = new VoxelTree8(); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.u16Tree = new VoxelTree16(); break;
            case VoxelIdSize::U20Bit: m_treeByIdSize.u20Tree = new VoxelTree20(); break;
        }
    }

    ~VoxelTree() {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: delete m_treeByIdSize.u8Tree; break;
            case VoxelIdSize::U16Bit: delete m_treeByIdSize.u16Tree; break;
            case VoxelIdSize::U20Bit: delete m_treeByIdSize.u20Tree; break;
        }
    }

    VoxelTree(VoxelTree&& other) noexcept {
        m_voxelIdSize = other.m_voxelIdSize;
        m_treeByIdSize = other.m_treeByIdSize;

        other.m_treeByIdSize.u8Tree = nullptr;
        other.m_treeByIdSize.u16Tree = nullptr;
        other.m_treeByIdSize.u20Tree = nullptr;
    }

    VoxelTree& operator=(VoxelTree&& other) noexcept {
        if (this != &other) {
            m_voxelIdSize = other.m_voxelIdSize;
            m_treeByIdSize = other.m_treeByIdSize;

            other.m_treeByIdSize.u8Tree = nullptr;
            other.m_treeByIdSize.u16Tree = nullptr;
            other.m_treeByIdSize.u20Tree = nullptr;
        }
        return *this;
    }

    VoxelTree(const VoxelTree& other) {
        m_voxelIdSize = other.m_voxelIdSize;
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.u8Tree = new VoxelTree8(*other.m_treeByIdSize.u8Tree); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.u16Tree = new VoxelTree16(*other.m_treeByIdSize.u16Tree); break;
            case VoxelIdSize::U20Bit: m_treeByIdSize.u20Tree = new VoxelTree20(*other.m_treeByIdSize.u20Tree); break;
        }
    }

    VoxelTree& operator=(const VoxelTree& other) {
        if (this != &other) {
            m_voxelIdSize = other.m_voxelIdSize;
            switch (m_voxelIdSize) {
                case VoxelIdSize::U8Bit: m_treeByIdSize.u8Tree = new VoxelTree8(*other.m_treeByIdSize.u8Tree); break;
                case VoxelIdSize::U16Bit: m_treeByIdSize.u16Tree = new VoxelTree16(*other.m_treeByIdSize.u16Tree); break;
                case VoxelIdSize::U20Bit: m_treeByIdSize.u20Tree = new VoxelTree20(*other.m_treeByIdSize.u20Tree); break;
            }
        }
        return *this;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: return m_treeByIdSize.u8Tree->getVoxel(x, y, z);
            case VoxelIdSize::U16Bit: return m_treeByIdSize.u16Tree->getVoxel(x, y, z);
            case VoxelIdSize::U20Bit: return m_treeByIdSize.u20Tree->getVoxel(x, y, z);
        }

        std::unreachable();
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: {
                bool success = m_treeByIdSize.u8Tree->trySetVoxel(x, y, z, voxel);
                if (!success) {
                    updateIdSize(VoxelIdSize::U16Bit);
                    m_treeByIdSize.u16Tree->trySetVoxel(x, y, z, voxel);
                }
                break;
            }
            case VoxelIdSize::U16Bit: {
                bool success = m_treeByIdSize.u16Tree->trySetVoxel(x, y, z, voxel);
                if (!success) {
                    updateIdSize(VoxelIdSize::U20Bit);
                    m_treeByIdSize.u20Tree->trySetVoxel(x, y, z, voxel);
                }
                break;
            }
            case VoxelIdSize::U20Bit: {
                bool success = m_treeByIdSize.u20Tree->trySetVoxel(x, y, z, voxel);
                assert(success && "Failed to set voxel in VoxelTree with U20Bit ID size");
                break;
            }
        }
    }

    [[nodiscard]] bool isEmpty() const {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: return m_treeByIdSize.u8Tree->isEmpty();
            case VoxelIdSize::U16Bit: return m_treeByIdSize.u16Tree->isEmpty();
            case VoxelIdSize::U20Bit: return m_treeByIdSize.u20Tree->isEmpty();
        }
        std::unreachable();
    }

    void serialize(std::ostream& os) {
        shrinkToMinimalIdSize();

        os.write(reinterpret_cast<const char*>(&m_voxelIdSize), sizeof(m_voxelIdSize));

        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.u8Tree->serialize(os); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.u16Tree->serialize(os); break;
            case VoxelIdSize::U20Bit: m_treeByIdSize.u20Tree->serialize(os); break;
        }
    }

    void deserialize(std::istream& is) {
        VoxelIdSize idSize;
        is.read(reinterpret_cast<char*>(&idSize), sizeof(idSize));

        updateIdSize(idSize);

        switch (idSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.u8Tree->deserialize(is); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.u16Tree->deserialize(is); break;
            case VoxelIdSize::U20Bit: m_treeByIdSize.u20Tree->deserialize(is); break;
        }
    }

    static size_t getMaxSerializedSize() {
        const size_t maxVoxelMapSize = VoxelMapper<VoxelIdSize::U20Bit>::getMaxSerializedSize();
        const size_t maxTreeSize = KTree<Depth, BaseSize, VoxelId<VoxelIdSize::U20Bit>>::getMaxSerializedSize();
        return sizeof(m_voxelIdSize) + maxVoxelMapSize + maxTreeSize;
    }

    void shrinkToMinimalIdSize() {
        VoxelIdSize optimalIdSize = m_voxelIdSize;
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: optimalIdSize = m_treeByIdSize.u8Tree->voxelMapper.getMinVoxelIdSize(); break;
            case VoxelIdSize::U16Bit: optimalIdSize = m_treeByIdSize.u16Tree->voxelMapper.getMinVoxelIdSize(); break;
            case VoxelIdSize::U20Bit: optimalIdSize = m_treeByIdSize.u20Tree->voxelMapper.getMinVoxelIdSize(); break;
        }

        updateIdSize(optimalIdSize);
    }

private:
    VoxelIdSize m_voxelIdSize{VoxelIdSize::U8Bit};
    VoxelTreeVariantT m_treeByIdSize;

    void updateIdSize(const VoxelIdSize newIdSize) {
        if (m_voxelIdSize == newIdSize) {
            return;
        }

        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U16Bit: {
                        auto* newTree = new VoxelTree16(m_treeByIdSize.u8Tree);
                        delete m_treeByIdSize.u8Tree;
                        m_treeByIdSize.u16Tree = newTree;
                        break;
                    }
                    case VoxelIdSize::U20Bit: {
                        auto* newTree = new VoxelTree20(m_treeByIdSize.u8Tree);
                        delete m_treeByIdSize.u8Tree;
                        m_treeByIdSize.u20Tree = newTree;
                        break;
                    }
                    default: std::unreachable();
                }
                break;
            }
            case VoxelIdSize::U16Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U8Bit: {
                        auto* newTree = new VoxelTree8(m_treeByIdSize.u16Tree);
                        delete m_treeByIdSize.u16Tree;
                        m_treeByIdSize.u8Tree = newTree;
                        break;
                    }
                    case VoxelIdSize::U20Bit: {
                        auto* newTree = new VoxelTree20(m_treeByIdSize.u16Tree);
                        delete m_treeByIdSize.u16Tree;
                        m_treeByIdSize.u20Tree = newTree;
                        break;
                    }
                    default: std::unreachable();
                }
                break;
            }
            case VoxelIdSize::U20Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U8Bit: {
                        auto* newTree = new VoxelTree8(m_treeByIdSize.u20Tree);
                        delete m_treeByIdSize.u20Tree;
                        m_treeByIdSize.u8Tree = newTree;
                        break;
                    }
                    case VoxelIdSize::U16Bit: {
                        auto* newTree = new VoxelTree16(m_treeByIdSize.u20Tree);
                        delete m_treeByIdSize.u20Tree;
                        m_treeByIdSize.u16Tree = newTree;
                        break;
                    }
                    default: std::unreachable();
                }
                break;
            }
        }

        m_voxelIdSize = newIdSize;
    }
};