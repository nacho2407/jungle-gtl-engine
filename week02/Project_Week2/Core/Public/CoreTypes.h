#pragma once

#include <map>
#include<string>
#include<vector>

typedef int int32;
typedef unsigned int uint32;

template<typename K, typename V>
using TMap = std::map<K, V>;

template<typename T>
using TArray = std::vector<T>;

typedef std::string FString;

using uint8 = unsigned char; // 1 byte
