#pragma once

#include <array>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using int32 = int;
using uint32 = unsigned int;
using uint8 = unsigned char;

template<typename K, typename V>
using TMap = std::map<K, V>;

template<typename T>
using TArray = std::vector<T>;

typedef std::string FString;

using uint8 = unsigned char; // 1 byte

// Double-ended Queue
template<typename T>
using TDeque = std::deque<T>;

// Ordered Set
template<typename T>
using TSet = std::set<T>;

// Ordered Map
template<typename K, typename V>
using TMap = std::map<K, V>;

// Hash Set
template<typename T>
using THashSet = std::unordered_set<T>;

// Hash Map
template<typename K, typename V>
using THashMap = std::unordered_map<K, V>;

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