#pragma once

#include <absl/container/flat_hash_set.h>

template <class T, class Hash = absl::DefaultHashContainerHash<T>,
          class Eq = absl::DefaultHashContainerEq<T>,
          class Allocator = std::allocator<T>>
using HashSet = absl::flat_hash_set<T, Hash, Eq, Allocator>;