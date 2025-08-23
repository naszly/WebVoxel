#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>

template<uint32_t SizeInBits, typename DataT = uint_fast32_t>
class Bitmap {
public:
    static constexpr uint32_t WORD_SIZE = sizeof(DataT);
    static constexpr uint32_t BITS_PER_WORD = WORD_SIZE * 8;

    Bitmap() {
        memset(m_data, 0, sizeof(m_data));
    }

    Bitmap(const Bitmap& other) {
        memcpy(m_data, other.m_data, sizeof(m_data));
    }

    void set(const uint32_t i) {
        assert(i < SizeInBits); [[assume(i < SizeInBits)]];
        m_data[i / BITS_PER_WORD] |= 1u << (i % BITS_PER_WORD);
    }

    void clear(const uint32_t i) {
        assert(i < SizeInBits); [[assume(i < SizeInBits)]];
        m_data[i / BITS_PER_WORD] &= ~(1u << (i % BITS_PER_WORD));
    }

    [[nodiscard]] bool test(const uint32_t i) const {
        assert(i < SizeInBits); [[assume(i < SizeInBits)]];
        return (m_data[i / BITS_PER_WORD] >> (i % BITS_PER_WORD)) & 1u;
    }

    [[nodiscard]] bool testWord(const uint32_t i) const {
        assert(i < DATA_SIZE); [[assume(i < DATA_SIZE)]];
        return m_data[i];
    }

    [[nodiscard]] char* data() {
        return reinterpret_cast<char*>(&m_data[0]);
    }

    [[nodiscard]] static size_t size() {
        return DATA_SIZE * sizeof(DataT);
    }

private:
    static constexpr uint32_t DATA_SIZE = (SizeInBits + BITS_PER_WORD - 1) / BITS_PER_WORD;
    DataT m_data[DATA_SIZE]{};
};