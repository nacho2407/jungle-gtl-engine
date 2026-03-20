#pragma once
#include <stdint.h>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <array>
#include <string>
#include <utility>

#include "Core/Debug.h"


typedef int int32;
typedef unsigned int uint32;

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
using TQueue = std::queue<T>;