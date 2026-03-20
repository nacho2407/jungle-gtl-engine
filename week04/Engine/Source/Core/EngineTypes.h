#pragma once

#include <cstdint>
#include<string>
#include<vector>
#include<list>
#include<unordered_set>
#include<unordered_map>
#include<array>
#include<queue>
#include<wrl.h>

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using FString = std::string;
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
using TMap = std::unordered_map<T0, T1>;

template <typename T0, typename T1>
using TPair = std::pair<T0, T1>;

template <typename T, int N>
using TStaticArray = std::array<T, N>;

template <typename T>
using TQueue = std::queue<T>;