// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include <stdint.h>
#include <cassert>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <array>
#include <string>
#include <utility>
#include <optional>

using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;

using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;

using FString = std::string;

template <typename T>
using TArray = std::vector<T>;

template <typename T>
using TDoubleLinkedList = std::list<T>;

template <typename T>
using TLinkedList = std::list<T>;

template <typename T, size_t N>
using TStaticArray = std::array<T, N>;


template <typename T>
using TSet = std::unordered_set<T>;

template <typename KeyType, typename ValueType>
using TMap = std::unordered_map<KeyType, ValueType>;

template <typename T1, typename T2>
using TPair = std::pair<T1, T2>;

template <typename T>
using TOptional = std::optional<T>;

template <typename T>
using TQueue = std::queue<T>;

// ===== Assert =====
#ifdef _DEBUG
#define ENGINE_CHECK(expr) assert(expr)
#define ENGINE_CHECK_FORMATTED(expr, msg) assert((expr) && (msg))
#else
#define ENGINE_CHECK(expr) ((void)0)
#define ENGINE_CHECK_FORMATTED(expr, msg) ((void)0)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
