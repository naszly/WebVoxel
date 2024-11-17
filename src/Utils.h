#pragma once
#include <climits>

namespace Utils {
    template <typename T>
    constexpr T pow(T base, T exp) {
        return (exp == 0) ? 1 : base * Utils::pow(base, exp - 1);
    }

    template <typename T>
    static T mod(const T &a, const T &b) {
        return (a % b + b) % b;
    }

    template <typename T>
    auto divideRoundDown(const T &numerator, const T &denominator)
    {
        return (numerator / denominator) + ((numerator % denominator) >> (sizeof(T) * CHAR_BIT - 1));
    }
}
