#pragma once

namespace Utils {
    template <typename T>
    constexpr T pow(T base, T exp) {
        return (exp == 0) ? 1 : base * Utils::pow(base, exp - 1);
    }
}
