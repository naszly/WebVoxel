#pragma once

#include "DataIdMapper.h"
#include "KTree.h"

template<typename DataType, uint32_t Depth, uint32_t BaseSize>
union DataTreeVariant {
    template<IdSize IdSize>
    struct DataTreeImpl {
        DataIdMapper<DataType, IdSize> mapper;
        KTree<Depth, BaseSize, IdType<IdSize>> tree;

        DataTreeImpl() = default;

        template<::IdSize OtherIdSize>
        explicit DataTreeImpl(const DataTreeImpl<OtherIdSize>* old)
            : mapper(old->mapper), tree(old->tree) { }

        [[nodiscard]] const DataType& getData(const uint32_t x, const uint32_t y, const uint32_t z) const {
            return mapper.getData(tree.get(x, y, z));
        }

        bool trySetData(const uint32_t x, const uint32_t y, const uint32_t z, const DataType &data) {
            auto [isSuccess, dataId] = mapper.tryGetId(data);
            if (isSuccess) {
                tree.set(x, y, z, dataId);
            }
            return isSuccess;
        }

        [[nodiscard]] bool isEmpty() const {
            return tree.isEmpty();
        }

        void serialize(std::ostream& os) {
            mapper.serialize(os);
            tree.serialize(os);
        }

        void deserialize(std::istream& is) {
            mapper.deserialize(is);
            tree.deserialize(is);
        }
    };

    DataTreeImpl<IdSize::U8Bit>* u8Tree{nullptr};
    DataTreeImpl<IdSize::U16Bit>* u16Tree;
    DataTreeImpl<IdSize::U20Bit>* u20Tree;
};
