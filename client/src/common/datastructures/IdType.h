#pragma once

#include <cstdint>
#include <type_traits>

enum class IdSize : uint8_t {
    U8Bit = 8,
    U16Bit = 16,
    U20Bit = 20
};

template<IdSize Size>
class IdType {
public:
    using UintT = std::conditional_t<Size == IdSize::U8Bit, uint8_t, std::conditional_t<Size == IdSize::U16Bit, uint16_t, uint32_t>>;

    IdType() = default;
    constexpr IdType(UintT id) : m_id(id) {}

    [[nodiscard]] bool isEmpty() const {
        return m_id == 0;
    }

    operator UintT() const {
        return m_id;
    }

    bool operator==(const IdType& other) const {
        return m_id == other.m_id;
    }

private:
    UintT m_id{0};
};

template<IdSize Size>
struct std::hash<IdType<Size>> {
    std::size_t operator()(const IdType<Size>& id) const noexcept {
        return std::hash<typename IdType<Size>::UintT>()(static_cast<IdType<Size>::UintT>(id));
    }
};
