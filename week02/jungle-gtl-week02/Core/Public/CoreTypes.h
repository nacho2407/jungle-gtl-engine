#pragma once

#include <array>
#include <deque>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using int32 = int;
using uint32 = unsigned int;
using uint8 = unsigned char;

template<typename T>
using TArray = std::vector<T>;

typedef std::string FString;

using uint8 = unsigned char; // 1 byte

// Double-ended Queue
template<typename T>
using TDeque = std::deque<T>;

// Hash Set
template<typename T>
using TSet = std::unordered_set<T>;

// Hash Map
template<typename K, typename V>
using TMap = std::unordered_map<K, V>;

// Pair
template<typename T1, typename T2>
using TPair = std::pair<T1, T2>;

// Static Array
template<typename T, size_t N>
using TStaticArray = std::array<T, N>;

// Queue
template<typename T>
using TQueue = std::queue<T>;

// Priority Queue
template<typename T>
using TPriorityQueue = std::priority_queue<T>;