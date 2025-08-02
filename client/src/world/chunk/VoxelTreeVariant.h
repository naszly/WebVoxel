#pragma once

#include "VoxelMapper.h"
#include "common/datastructures/KTree.h"

template<uint32_t Depth, uint32_t BaseSize>
union VoxelTreeVariant {
    template<VoxelIdSize VoxelIdSize>
    struct VoxelTreeImpl {
        VoxelMapper<VoxelIdSize> voxelMapper;
        KTree<Depth, BaseSize, VoxelId<VoxelIdSize>> tree;

        VoxelTreeImpl() = default;

        template<::VoxelIdSize OtherVoxelIdSize>
        explicit VoxelTreeImpl(const VoxelTreeImpl<OtherVoxelIdSize>* old)
            : voxelMapper(old->voxelMapper), tree(old->tree) { }

        [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
            return voxelMapper.getVoxelData(tree.get(x, y, z));
        }

        bool trySetVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
            auto [isSuccess, voxelId] = voxelMapper.tryGetVoxelId(voxel);
            if (isSuccess) {
                tree.set(x, y, z, voxelId);
            }
            return isSuccess;
        }

        [[nodiscard]] bool isEmpty() const {
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

    VoxelTreeImpl<VoxelIdSize::U8Bit>* u8Tree{nullptr};
    VoxelTreeImpl<VoxelIdSize::U16Bit>* u16Tree;
    VoxelTreeImpl<VoxelIdSize::U20Bit>* u20Tree;
};
