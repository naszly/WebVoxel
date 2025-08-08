#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>

template<uint32_t Size, typename DataT = uint_fast32_t>
class Bitmap {
public:
    static constexpr uint32_t WORD_SIZE = sizeof(DataT);

    Bitmap() {
        memset(m_data, 0, sizeof(m_data));
    }

    Bitmap(const Bitmap& other) {
        memcpy(m_data, other.m_data, sizeof(m_data));
    }

    void set(const uint32_t i) {
        assert(i < Size); [[assume(i < Size)]];
        m_data[i / WORD_SIZE] |= 1u << (i % WORD_SIZE);
    }

    void clear(const uint32_t i) {
        assert(i < Size); [[assume(i < Size)]];
        m_data[i / WORD_SIZE] &= ~(1u << (i % WORD_SIZE));
    }

    [[nodiscard]] bool test(const uint32_t i) const {
        assert(i < Size); [[assume(i < Size)]];
        return (m_data[i / WORD_SIZE] >> (i % WORD_SIZE)) & 1u;
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
    static constexpr uint32_t DATA_SIZE = Size / WORD_SIZE + (Size % WORD_SIZE != 0);
    DataT m_data[DATA_SIZE]{};
};