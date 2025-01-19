#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>

template<uint32_t SIZE>
class Bitmap {
    using data_t = uint_fast32_t;
public:
    static constexpr uint32_t WORD_SIZE = sizeof(data_t);

    Bitmap() {
        memset(data, 0, sizeof(data));
    }

    void set(const uint32_t i) {
        assert(i < SIZE); [[assume(i < SIZE)]];
        data[i / WORD_SIZE] |= 1u << (i % WORD_SIZE);
    }

    void clear(const uint32_t i) {
        assert(i < SIZE); [[assume(i < SIZE)]];
        data[i / WORD_SIZE] &= ~(1u << (i % WORD_SIZE));
    }

    [[nodiscard]] bool test(const uint32_t i) const {
        assert(i < SIZE); [[assume(i < SIZE)]];
        return (data[i / WORD_SIZE] >> (i % WORD_SIZE)) & 1u;
    }

    [[nodiscard]] bool testWord(const uint32_t i) const {
        assert(i < DATA_SIZE); [[assume(i < DATA_SIZE)]];
        return data[i];
    }

private:
    static constexpr uint32_t DATA_SIZE = SIZE / WORD_SIZE + (SIZE % WORD_SIZE != 0);
    data_t data[DATA_SIZE]{};
};