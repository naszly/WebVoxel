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

    bool isEmpty() const {
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
    friend class DataIdMapper<DataT, IdSize::U8Bit>;
    friend class DataIdMapper<DataT, IdSize::U16Bit>;
    friend class DataIdMapper<DataT, IdSize::U20Bit>;
    using IdType = ::IdType<Size>;
    static constexpr size_t MAX_DATA = Utils::pow(static_cast<size_t>(2), static_cast<size_t>(Size));
    static constexpr DataT EMPTY_DATA{};

public:
    DataIdMapper() {
        m_data.push_back(EMPTY_DATA);
        m_dataIdMap[EMPTY_DATA] = IdType(0);
    }

    template<IdSize OldIdSize>
    explicit DataIdMapper(const DataIdMapper<DataT, OldIdSize>& other)
        : m_data(other.m_data.begin(), other.m_data.end()) {
        assert(m_data.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
    }

    template<IdSize OldIdSize>
    DataIdMapper& operator=(const DataIdMapper<DataT, OldIdSize>& other) {
        m_data.assign(other.m_data.begin(), other.m_data.end());
        assert(m_data.size() <= MAX_DATA && "DataIdMapper size exceeds maximum allowed size");
        return *this;
    }

    [[nodiscard]] const DataT& getData(const IdType id) const {
        if (id < m_data.size()) {
            return m_data[id];
        }
        return EMPTY_DATA;
    }

    std::pair<bool, IdType> tryGetId(const DataT &data) {
        if (auto it = m_dataIdMap.find(data); it != m_dataIdMap.end()) {
            return {true, it->second};
        }
        if (m_data.size() < MAX_DATA) {
            m_data.push_back(data);
            m_dataIdMap[data] = static_cast<IdType>(m_data.size() - 1);
            return {true, static_cast<IdType>(m_data.size() - 1)};
        }
        return {false, IdType()};
    }

    [[nodiscard]] bool isEmpty() const {
        return m_data.empty();
    }

    void serialize(std::ostream& os) const {
        const auto size = static_cast<IdType>(m_data.size());
        os.write(reinterpret_cast<const char*>(&size), sizeof(IdType));
        os.write(reinterpret_cast<const char*>(m_data.data()), size * sizeof(DataT));
    }

    void deserialize(std::istream& is) {
        IdType size;
        is.read(reinterpret_cast<char*>(&size), sizeof(IdType));
        m_data.resize(size);
        is.read(reinterpret_cast<char*>(m_data.data()), size * sizeof(DataT));

        m_dataIdMap.clear();
        for (size_t i = 0; i < m_data.size(); ++i) {
            m_dataIdMap[m_data[i]] = static_cast<IdType>(i);
        }
    }

    static size_t getMaxSerializedSize() {
        return sizeof(DataT) * MAX_DATA;
    }

    [[nodiscard]] IdSize getMinIdSize() const {
        if (m_data.size() < DataIdMapper<DataT, IdSize::U8Bit>::MAX_DATA) {
            return IdSize::U8Bit;
        }
        if (m_data.size() < DataIdMapper<DataT, IdSize::U16Bit>::MAX_DATA) {
            return IdSize::U16Bit;
        }
        return IdSize::U20Bit;
    }

private:
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
    std::vector<DataT> m_data;
    std::unordered_map<DataT, IdType, DataHasher> m_dataIdMap;
};
