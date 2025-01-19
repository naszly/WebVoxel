#pragma once

#include <cstdint>
#include <cassert>
#include <algorithm>

template<uint32_t SIZE>
class Bitmap {
public:
    Bitmap() {
        std::fill_n(&data[0], SIZE / sizeof(data_t), 0);
    }

    void set(const uint32_t i) {
        assert(i < SIZE); [[assume(i < SIZE)]];
        data[i / sizeof(data_t)] |= 1 << (i % sizeof(data_t));
    }

    void clear(const uint32_t i) {
        assert(i < SIZE); [[assume(i < SIZE)]];
        data[i / sizeof(data_t)] &= ~(1 << (i % sizeof(data_t)));
    }

    [[nodiscard]] bool test(const uint32_t i) const {
        assert(i < SIZE); [[assume(i < SIZE)]];
        return data[i / sizeof(data_t)] & (1 << (i % sizeof(data_t)));
    }

private:
    using data_t = uint8_t;
    data_t data[SIZE / sizeof(data_t)]{};
};