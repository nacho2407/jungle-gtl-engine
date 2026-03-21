#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <array>
#include <queue>
#include <wrl.h>

using uint8 = std::uint8_t;
using uint64 = std::uint64_t;

typedef int int32;
typedef unsigned int uint32;

using FString = std::string; // UTF-8 assumed
using FU32String = std::u32string; // C++ 20 표준에 std::string과 std::u32string 간 변환 표준 API가 없음(deprecated)
using WFString = std::wstring;

template <typename T>
using TComPtr = Microsoft::WRL::ComPtr<T>;

template <typename T>
using TArray = std::vector<T>;

template <typename T>
using TLinkedList = std::list<T>;

template <typename T>
using TSet = std::unordered_set<T>;

template <typename T0, typename T1>
using TMap = std::map<T0, T1>;

template <typename T0, typename T1>
using TUMap = std::unordered_map<T0, T1>;

template <typename T1, typename T2>
using TPair = std::pair<T1, T2>;

template <typename T1, int N>
using TStaticArray = std::array<T1, N>;

template <typename T>
using TQueue = std::queue<T>;

template<typename T, typename Container = TArray<T>, typename Compare = std::less<T>>
using TPriorityQueue = std::priority_queue<T, Container, Compare>;