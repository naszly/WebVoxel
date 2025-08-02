#pragma once

#include "DataIdMapper.h"
#include "KTree.h"

template<typename DataType, uint32_t Depth, uint32_t BaseSize, IdSize IdSize>
class IdMappedKTree {
public:
    IdMappedKTree() = default;

    template<::IdSize OtherIdSize>
    explicit IdMappedKTree(const IdMappedKTree<DataType, Depth, BaseSize, OtherIdSize>& old)
        : m_mapper(old.getMapper()), m_tree(old.getTree()) { }

    [[nodiscard]] const DataType& getData(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_mapper.getData(m_tree.get(x, y, z));
    }

    bool trySetData(const uint32_t x, const uint32_t y, const uint32_t z, const DataType &data) {
        auto [isSuccess, dataId] = m_mapper.tryGetId(data);
        if (isSuccess) {
            m_tree.set(x, y, z, dataId);
        }
        return isSuccess;
    }

    [[nodiscard]] bool isEmpty() const {
        return m_tree.isEmpty();
    }

    void serialize(std::ostream& os) {
        m_mapper.serialize(os);
        m_tree.serialize(os);
    }

    void deserialize(std::istream& is) {
        m_mapper.deserialize(is);
        m_tree.deserialize(is);
    }

    static size_t getMaxSerializedSize() {
        const size_t dataIdMapperSize = DataIdMapper<DataType, IdSize>::getMaxSerializedSize();
        const size_t kTreeSize = KTree<Depth, BaseSize, IdType<IdSize>>::getMaxSerializedSize();
        return dataIdMapperSize + kTreeSize;
    }

    [[nodiscard]] ::IdSize getMinIdSize() const { return m_mapper.getMinIdSize(); }

    const DataIdMapper<DataType, IdSize>& getMapper() const { return m_mapper; }
    const KTree<Depth, BaseSize, IdType<IdSize>>& getTree() const { return m_tree; }

private:
    DataIdMapper<DataType, IdSize> m_mapper;
    KTree<Depth, BaseSize, IdType<IdSize>> m_tree;
};
