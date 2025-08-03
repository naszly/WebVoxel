#pragma once

#include <ostream>
#include <istream>
#include <vector>

#include "HashMap.h"
#include "IdType.h"
#include "common/Utils.h"

template<typename DataT, IdSize Size>
class DataIdMapper {
    struct DataHasher;
    using IdType = ::IdType<Size>;
    static constexpr DataT EMPTY_DATA{};
public:
    static constexpr size_t MAX_DATA = Utils::pow(static_cast<size_t>(2), static_cast<size_t>(Size));

    static std::vector<DataT> createEmptyDataVector() {
        return std::vector<DataT>{EMPTY_DATA};
    }

    DataIdMapper() {
        m_dataVector = createEmptyDataVector();
        m_dataIdMap[EMPTY_DATA] = IdType(0);
    }

    explicit DataIdMapper(std::vector<DataT>&& dataVector) : m_dataVector(std::move(dataVector)) {
        assert(m_dataVector.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
        rebuildDataIdMap();
    }

    template<IdSize OtherIdSize>
    explicit DataIdMapper(const DataIdMapper<DataT, OtherIdSize>& other) : m_dataVector(other.getDataVector()) {
        assert(m_dataVector.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
        rebuildDataIdMap();
    }

    [[nodiscard]] const DataT& getData(const IdType id) const {
        if (id < m_dataVector.size()) {
            return m_dataVector[id];
        }
        return EMPTY_DATA;
    }

    std::pair<bool, IdType> tryGetId(const DataT &data) {
        if (auto it = m_dataIdMap.find(data); it != m_dataIdMap.end()) {
            return {true, it->second};
        }
        if (m_dataVector.size() < MAX_DATA) {
            m_dataVector.push_back(data);
            m_dataIdMap[data] = static_cast<IdType>(m_dataVector.size() - 1);
            return {true, static_cast<IdType>(m_dataVector.size() - 1)};
        }
        return {false, IdType()};
    }

    IdType getId(const DataT& data) const {
        return m_dataIdMap.at(data);
    }

    [[nodiscard]] bool isEmpty() const {
        return m_dataVector.empty();
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

    static size_t getMaxSerializedSize() {
        return sizeof(DataT) * MAX_DATA;
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
    HashMap<DataT, IdType, DataHasher> m_dataIdMap;

    void rebuildDataIdMap() {
        assert(m_dataVector.front() == EMPTY_DATA);
        m_dataIdMap.clear();
        for (size_t i = 0; i < m_dataVector.size(); ++i) {
            m_dataIdMap[m_dataVector[i]] = static_cast<IdType>(i);
        }
    }

    struct DataHasher {
        size_t operator()(const DataT& value) const {
            constexpr size_t s = sizeof(DataT);
            if constexpr (s == 1) {
                return absl::Hash<uint8_t>()(*reinterpret_cast<const uint8_t*>(&value));
            } else if constexpr (s == 2) {
                return absl::Hash<uint16_t>()(*reinterpret_cast<const uint16_t*>(&value));
            } else if constexpr (s == 4) {
                return absl::Hash<uint32_t>()(*reinterpret_cast<const uint32_t*>(&value));
            } else if constexpr (s == 8) {
                return absl::Hash<uint64_t>()(*reinterpret_cast<const uint64_t*>(&value));
            } else {
                static_assert(s == 1 || s == 2 || s == 4 || s == 8, "Unsupported DataT size for hashing");
                std::unreachable();
            }
        }
    };
};
