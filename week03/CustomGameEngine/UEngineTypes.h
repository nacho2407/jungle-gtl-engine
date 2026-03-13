#pragma once

#include <vector>
#include <unordered_map>

typedef int int32;
typedef unsigned int uint32;

template <typename T>
using TArray = std::vector<T>;

template <typename T0, typename T1>
using TMap = std::unordered_map<T0, T1>;
