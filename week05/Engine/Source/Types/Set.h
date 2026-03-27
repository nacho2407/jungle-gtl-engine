#pragma once
#include <memory>
#include <unordered_set>

template <
    typename T,
    typename Hasher = std::hash<T>,
    typename KeyEqual = std::equal_to<T>,
    typename Allocator = std::allocator<T>>
using TSet = std::unordered_set<T, Hasher, KeyEqual, Allocator>;
