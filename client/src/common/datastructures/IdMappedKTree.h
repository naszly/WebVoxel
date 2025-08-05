#pragma once

#include "HashMap.h"
#include "HashSet.h"
#include "DataIdMapper.h"
#include "KTree.h"

template<typename DataType, uint32_t Depth, uint32_t BaseSize, IdSize IdSize>
class IdMappedKTree {
    using DataIdMapper = ::DataIdMapper<DataType, IdSize>;
    using IdHashSet = HashSet<IdType<IdSize>>;
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

    void optimizeDataToIdMapping() {
        auto activeIds = collectActiveIds();
        auto newMapper = buildOptimizedMapper(activeIds);
        remapTreeIds(activeIds, newMapper);
        m_mapper = std::move(newMapper);
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

    [[nodiscard]] ::IdSize getMinIdSize() const { return m_mapper.getMinIdSize(); }

    const DataIdMapper& getMapper() const { return m_mapper; }
    const KTree<Depth, BaseSize, IdType<IdSize>>& getTree() const { return m_tree; }

private:
    DataIdMapper m_mapper;
    KTree<Depth, BaseSize, IdType<IdSize>> m_tree;

    IdHashSet collectActiveIds() const {
        IdHashSet activeIds;
        m_tree.forEach([&](const IdType<IdSize>& id) {
            activeIds.insert(id);
        });
        return activeIds;
    }

    DataIdMapper buildOptimizedMapper(const IdHashSet& activeIds) const {
        std::vector<DataType> activeData = DataIdMapper::createEmptyDataVector();
        for (const auto& id : activeIds) {
            activeData.push_back(m_mapper.getData(id));
        }
        return DataIdMapper(std::move(activeData));
    }

    void remapTreeIds(const IdHashSet& activeIds, DataIdMapper& newMapper) const {
        HashMap<IdType<IdSize>, IdType<IdSize>> idRemap;
        for (const auto& id : activeIds) {
            const auto& data = m_mapper.getData(id);
            idRemap[id] = newMapper.getId(data);
        }
        m_tree.forEach([&](IdType<IdSize>& id) {
            id = idRemap.at(id);
        });
    }
};
