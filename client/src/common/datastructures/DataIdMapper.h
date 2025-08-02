#pragma once

#include <ostream>
#include <istream>
#include <vector>
#include <unordered_map>

#include "common/Utils.h"

enum class IdSize : uint8_t {
    U8Bit = 8,
    U16Bit = 16,
    U20Bit = 20
};

template<IdSize Size>
class IdType {
public:
    using UintT = std::conditional_t<
        Size == IdSize::U8Bit,
        uint8_t,
        std::conditional_t<
            Size == IdSize::U16Bit,
            uint16_t,
            uint32_t
        >
    >;

    IdType() = default;
    IdType(UintT id) : m_id(id) {}

    [[nodiscard]] bool isEmpty() const {
        return m_id == 0;
    }

    operator UintT() const {
        return m_id;
    }

private:
    UintT m_id{0};
};

template<typename DataT, IdSize Size>
class DataIdMapper {
    struct DataHasher;
    using IdType = ::IdType<Size>;
    static constexpr DataT EMPTY_DATA{};
public:
    static constexpr size_t MAX_DATA = Utils::pow(static_cast<size_t>(2), static_cast<size_t>(Size));

    DataIdMapper() {
        m_dataVector.push_back(EMPTY_DATA);
        m_dataIdMap[EMPTY_DATA] = IdType(0);
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
    std::unordered_map<DataT, IdType, DataHasher> m_dataIdMap;

    void rebuildDataIdMap() {
        assert(m_dataVector.front() == EMPTY_DATA);
        m_dataIdMap.clear();
        for (size_t i = 0; i < m_dataVector.size(); ++i) {
            m_dataIdMap[m_dataVector[i]] = static_cast<IdType>(i);
        }
    }

    struct DataHasher {
        std::size_t operator()(const DataT& data) const {
            static_assert(std::is_trivially_copyable_v<DataT>, "DataT must be trivially copyable");
            constexpr size_t sz = sizeof(DataT);
            if constexpr (sz == 1) {
                return std::hash<uint8_t>()(*reinterpret_cast<const uint8_t*>(&data));
            } else if constexpr (sz == 2) {
                return std::hash<uint16_t>()(*reinterpret_cast<const uint16_t*>(&data));
            } else if constexpr (sz == 4) {
                return std::hash<uint32_t>()(*reinterpret_cast<const uint32_t*>(&data));
            } else if constexpr (sz == 8) {
                return std::hash<uint64_t>()(*reinterpret_cast<const uint64_t*>(&data));
            } else {
                static_assert(sz <= 8, "DataT size must be 1, 2, 4, or 8 bytes");
                return 0;
            }
        }
    };
};
