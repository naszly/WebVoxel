#pragma once

#include <absl/container/flat_hash_map.h>

template <class K, class V, class Hash = absl::DefaultHashContainerHash<K>,
          class Eq = absl::DefaultHashContainerEq<K>,
          class Allocator = std::allocator<std::pair<const K, V>>>
using HashMap = absl::flat_hash_map<K, V, Hash, Eq, Allocator>;