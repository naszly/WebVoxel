#pragma once

#include "common/DataIdMapper.h"
#include "common/datastructures/KTree.h"

template<uint32_t Depth, uint32_t BaseSize>
union VoxelTreeVariant {
    template<IdSize IdSize>
    struct VoxelTreeImpl {
        DataIdMapper<VoxelData, IdSize> voxelMapper;
        KTree<Depth, BaseSize, IdType<IdSize>> tree;

        VoxelTreeImpl() = default;

        template<::IdSize OtherVoxelIdSize>
        explicit VoxelTreeImpl(const VoxelTreeImpl<OtherVoxelIdSize>* old)
            : voxelMapper(old->voxelMapper), tree(old->tree) { }

        [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
            return voxelMapper.getData(tree.get(x, y, z));
        }

        bool trySetVoxel(const uint32_t x, const uint32_t y, const uint32_t z, const VoxelData &voxel) {
            auto [isSuccess, voxelId] = voxelMapper.tryGetId(voxel);
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

    VoxelTreeImpl<IdSize::U8Bit>* u8Tree{nullptr};
    VoxelTreeImpl<IdSize::U16Bit>* u16Tree;
    VoxelTreeImpl<IdSize::U20Bit>* u20Tree;
};
