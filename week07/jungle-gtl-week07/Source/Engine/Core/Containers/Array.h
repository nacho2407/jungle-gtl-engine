#pragma once
#include <memory>
#include <vector>

template <typename T, typename Allocator = std::allocator<T>>
using TArray = std::vector<T, Allocator>;
