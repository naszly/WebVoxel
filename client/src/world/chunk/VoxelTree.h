#pragma once

#include "VoxelData.h"
#include "VoxelMapper.h"
#include "common/datastructures/KTree.h"
#include "common/Utils.h"

template<uint32_t Depth, uint32_t BaseSize>
class VoxelTree {
    constexpr static uint32_t SIZE = Utils::pow(BaseSize, Depth);
    static constexpr uint32_t BITMAP_SIZE = SIZE + 2;

    union TreeByIdSizeUnion {
        template<VoxelIdSize VoxelIdSize>
        struct VoxelMapperAndTree {
            VoxelMapper<VoxelIdSize> voxelMapper;
            KTree<Depth, BaseSize, VoxelId<VoxelIdSize>> tree;

            VoxelMapperAndTree() = default;

            template<::VoxelIdSize OldVoxelIdSize>
            explicit VoxelMapperAndTree(const VoxelMapperAndTree<OldVoxelIdSize>* old)
                : voxelMapper(old->voxelMapper) {
                tree.copyFrom(old->tree);
            }

            const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
                return voxelMapper.getVoxelData(tree.get(x, y, z));
            }

            bool trySetVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
                auto [isSuccess, voxelId] = voxelMapper.tryGetVoxelId(voxel);
                if (isSuccess) {
                    tree.set(x, y, z, voxelId);
                }
                return isSuccess;
            }

            bool isEmpty() const {
                return tree.isEmpty();
            }

            void serialize(std::ostream& os) {
                voxelMapper.serialize(os);
                tree.serialize(os);
            }

            void deserialize(std::istream& is) {
                voxelMapper.deserialize(is);
                tree.deserialize(is);
            }
        };
        VoxelMapperAndTree<VoxelIdSize::U8Bit>* idSize8;
        VoxelMapperAndTree<VoxelIdSize::U16Bit>* idSize16;
        VoxelMapperAndTree<VoxelIdSize::U32Bit>* idSize32;
    } m_treeByIdSize;

    using VoxelTree8 = typename TreeByIdSizeUnion::template VoxelMapperAndTree<VoxelIdSize::U8Bit>;
    using VoxelTree16 = typename TreeByIdSizeUnion::template VoxelMapperAndTree<VoxelIdSize::U16Bit>;
    using VoxelTree32 = typename TreeByIdSizeUnion::template VoxelMapperAndTree<VoxelIdSize::U32Bit>;
public:
    VoxelTree() {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit:
                m_treeByIdSize.idSize8 = new VoxelTree8();
                break;
            case VoxelIdSize::U16Bit:
                m_treeByIdSize.idSize16 = new VoxelTree16();
                break;
            case VoxelIdSize::U32Bit:
                m_treeByIdSize.idSize32 = new VoxelTree32();
                break;
        }
    }

    ~VoxelTree() {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit:
                delete m_treeByIdSize.idSize8;
                break;
            case VoxelIdSize::U16Bit:
                delete m_treeByIdSize.idSize16;
                break;
            case VoxelIdSize::U32Bit:
                delete m_treeByIdSize.idSize32;
                break;
        }
    }

    VoxelTree(VoxelTree&& other) noexcept {
        m_voxelIdSize = other.m_voxelIdSize;
        m_treeByIdSize = other.m_treeByIdSize;

        other.m_treeByIdSize.idSize8 = nullptr;
        other.m_treeByIdSize.idSize16 = nullptr;
        other.m_treeByIdSize.idSize32 = nullptr;
    }

    VoxelTree& operator=(VoxelTree&& other) noexcept {
        if (this != &other) {
            m_voxelIdSize = other.m_voxelIdSize;
            m_treeByIdSize = other.m_treeByIdSize;

            other.m_treeByIdSize.idSize8 = nullptr;
            other.m_treeByIdSize.idSize16 = nullptr;
            other.m_treeByIdSize.idSize32 = nullptr;
        }
        return *this;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: return m_treeByIdSize.idSize8->getVoxel(x, y, z);
            case VoxelIdSize::U16Bit: return m_treeByIdSize.idSize16->getVoxel(x, y, z);
            case VoxelIdSize::U32Bit: return m_treeByIdSize.idSize32->getVoxel(x, y, z);
        }

        assert(false && "Invalid voxel ID size");
    }

    void setVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: {
                bool success = m_treeByIdSize.idSize8->trySetVoxel(x, y, z, voxel);
                if (!success) {
                    updateIdSize(VoxelIdSize::U16Bit);
                    m_treeByIdSize.idSize16->trySetVoxel(x, y, z, voxel);
                }
                break;
            }
            case VoxelIdSize::U16Bit: {
                bool success = m_treeByIdSize.idSize16->trySetVoxel(x, y, z, voxel);
                if (!success) {
                    updateIdSize(VoxelIdSize::U32Bit);
                    m_treeByIdSize.idSize32->trySetVoxel(x, y, z, voxel);
                }
                break;
            }
            case VoxelIdSize::U32Bit: {
                bool success = m_treeByIdSize.idSize32->trySetVoxel(x, y, z, voxel);
                assert(success && "Failed to set voxel in VoxelTree with U32Bit ID size");
                break;
            }
        }
    }

    [[nodiscard]] bool isEmpty() const {
        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: return m_treeByIdSize.idSize8->isEmpty();
            case VoxelIdSize::U16Bit: return m_treeByIdSize.idSize16->isEmpty();
            case VoxelIdSize::U32Bit: return m_treeByIdSize.idSize32->isEmpty();
        }
        assert(false && "Invalid voxel ID size");
        return true;
    }

    void serialize(std::ostream& os) {
        os.write(reinterpret_cast<const char*>(&m_voxelIdSize), sizeof(m_voxelIdSize));

        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.idSize8->serialize(os); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.idSize16->serialize(os); break;
            case VoxelIdSize::U32Bit: m_treeByIdSize.idSize32->serialize(os); break;
        }
    }

    void deserialize(std::istream& is) {
        VoxelIdSize idSize;
        is.read(reinterpret_cast<char*>(&idSize), sizeof(idSize));

        updateIdSize(idSize);

        switch (idSize) {
            case VoxelIdSize::U8Bit: m_treeByIdSize.idSize8->deserialize(is); break;
            case VoxelIdSize::U16Bit: m_treeByIdSize.idSize16->deserialize(is); break;
            case VoxelIdSize::U32Bit: m_treeByIdSize.idSize32->deserialize(is); break;
        }
    }

    static size_t getMaxSerializedSize() {
        const size_t maxVoxelMapSize = VoxelMapper<VoxelIdSize::U32Bit>::getMaxSerializedSize();
        const size_t maxTreeSize = KTree<Depth, BaseSize, VoxelId<VoxelIdSize::U32Bit>>::getMaxSerializedSize();
        return sizeof(m_voxelIdSize) + maxVoxelMapSize + maxTreeSize;
    }

private:
    VoxelIdSize m_voxelIdSize{VoxelIdSize::U8Bit};

    void updateIdSize(const VoxelIdSize newIdSize) {
        if (m_voxelIdSize == newIdSize) {
            return;
        }

        switch (m_voxelIdSize) {
            case VoxelIdSize::U8Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U16Bit: {
                        auto* newTree = new VoxelTree16(m_treeByIdSize.idSize8);
                        delete m_treeByIdSize.idSize8;
                        m_treeByIdSize.idSize16 = newTree;
                        break;
                    }
                    case VoxelIdSize::U32Bit: {
                        auto* newTree = new VoxelTree32(m_treeByIdSize.idSize8);
                        delete m_treeByIdSize.idSize8;
                        m_treeByIdSize.idSize32 = newTree;
                        break;
                    }
                    default: assert(false && "Invalid voxel ID size transition");
                }
                break;
            }
            case VoxelIdSize::U16Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U8Bit: {
                        auto* newTree = new VoxelTree8(m_treeByIdSize.idSize16);
                        delete m_treeByIdSize.idSize16;
                        m_treeByIdSize.idSize8 = newTree;
                        break;
                    }
                    case VoxelIdSize::U32Bit: {
                        auto* newTree = new VoxelTree32(m_treeByIdSize.idSize16);
                        delete m_treeByIdSize.idSize16;
                        m_treeByIdSize.idSize32 = newTree;
                        break;
                    }
                    default: assert(false && "Invalid voxel ID size transition");
                }
                break;
            }
            case VoxelIdSize::U32Bit: {
                switch (newIdSize) {
                    case VoxelIdSize::U8Bit: {
                        auto* newTree = new VoxelTree8(m_treeByIdSize.idSize32);
                        delete m_treeByIdSize.idSize32;
                        m_treeByIdSize.idSize8 = newTree;
                        break;
                    }
                    case VoxelIdSize::U16Bit: {
                        auto* newTree = new VoxelTree16(m_treeByIdSize.idSize32);
                        delete m_treeByIdSize.idSize32;
                        m_treeByIdSize.idSize16 = newTree;
                        break;
                    }
                    default: assert(false && "Invalid voxel ID size transition");
                }
                break;
            }
        }

        m_voxelIdSize = newIdSize;
    }
};