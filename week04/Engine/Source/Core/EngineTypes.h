#pragma once

#include<string>
#include<vector>
#include<list>
#include<unordered_set>
#include<unordered_map>
#include<array>
#include<queue>
#include<wrl.h>

typedef int int32;
typedef unsigned int uint32;

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