#pragma once

#include <concepts>

template<typename T>
concept HasEmptyTrait = requires(T t) {
    { t.isEmpty() } -> std::convertible_to<bool>;
};