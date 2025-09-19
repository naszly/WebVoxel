#pragma once

#include <ostream>
#include <istream>
#include <vector>
#include <type_traits>

#include "HashMap.h"
#include "IdType.h"
#include "common/Utils.h"

template<typename DataT, IdSize Size>
class DataIdMapper {
    struct DataHasher;
    using IdType = ::IdType<Size>;
    static constexpr DataT EMPTY_DATA{};
    static_assert(std::is_trivially_copyable_v<DataT>, "DataIdMapper requires DataT to be trivially copyable for safe serialization/deserialization.");
public:
    static constexpr size_t MAX_DATA = Utils::pow(static_cast<size_t>(2), static_cast<size_t>(Size));

    static std::vector<DataT> createEmptyDataVector() {
        return std::vector<DataT>{EMPTY_DATA};
    }

    DataIdMapper() : m_dataVector{EMPTY_DATA} {}

    explicit DataIdMapper(std::vector<DataT>&& dataVector) : m_dataVector(std::move(dataVector)) {
        assert(m_dataVector.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
        rebuildDataIdMap();
    }

    template<IdSize OtherIdSize>
    explicit DataIdMapper(const DataIdMapper<DataT, OtherIdSize>& other) : m_dataVector(other.getDataVector()) {
        assert(m_dataVector.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
        rebuildDataIdMap();
    }

    [[nodiscard]] const DataT& getData(const IdType id) const noexcept {
        if (id < m_dataVector.size()) {
            return m_dataVector[id];
        }
        return EMPTY_DATA;
    }

    std::pair<bool, IdType> tryGetId(const DataT &data) {
        if (data == EMPTY_DATA) {
            return {true, IdType(0)};
        }
        if (auto it = m_dataIdMap.find(data); it != m_dataIdMap.end()) {
            return {true, it->second};
        }
        if (m_dataVector.size() < MAX_DATA) {
            m_dataVector.push_back(data);
            const IdType newId = static_cast<IdType>(m_dataVector.size() - 1);
            m_dataIdMap.emplace(data, newId);
            return {true, newId};
        }
        return {false, IdType()};
    }

    IdType getId(const DataT& data) const {
        if (data == EMPTY_DATA) {
            return IdType(0);
        }
        return m_dataIdMap.at(data);
    }

    void serialize(std::ostream& os) const {
        const auto size = static_cast<IdType>(m_dataVector.size());
        os.write(reinterpret_cast<const char*>(&size), sizeof(IdType));
        os.write(reinterpret_cast<const char*>(m_dataVector.data()), size * sizeof(DataT));
    }

    void deserialize(std::istream& is) {
        IdType size;
        is.read(reinterpret_cast<char*>(&size), sizeof(IdType));
        m_dataVector.resize(size);
        is.read(reinterpret_cast<char*>(m_dataVector.data()), size * sizeof(DataT));

        rebuildDataIdMap();
    }

    [[nodiscard]] IdSize getMinIdSize() const {
        if (m_dataVector.size() < DataIdMapper<DataT, IdSize::U8Bit>::MAX_DATA) {
            return IdSize::U8Bit;
        }
        if (m_dataVector.size() < DataIdMapper<DataT, IdSize::U16Bit>::MAX_DATA) {
            return IdSize::U16Bit;
        }
        return IdSize::U20Bit;
    }

    const std::vector<DataT>& getDataVector() const {
        return m_dataVector;
    }

private:
    std::vector<DataT> m_dataVector;
    HashMap<DataT, IdType> m_dataIdMap;

    void rebuildDataIdMap() {
        assert(m_dataVector.front() == EMPTY_DATA);
        m_dataIdMap.clear();
        if (m_dataVector.size() > 1) {
            m_dataIdMap.reserve(m_dataVector.size() - 1);
        }
        for (size_t i = 1; i < m_dataVector.size(); ++i) {
            m_dataIdMap.emplace(m_dataVector[i], static_cast<IdType>(i));
        }
    }
};
