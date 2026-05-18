#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"

#include <unordered_map>
#include <functional>
#include <utility>

template <typename KeyType>
struct TDefaultMapHash
{
    size_t operator()(const KeyType& Key) const
    {
        return static_cast<size_t>(GetTypeHash(Key));
    }
};
template <
	typename KeyType,
	typename ValueType,
    typename Hasher = TDefaultMapHash<KeyType>,
	typename KeyEqual = std::equal_to<KeyType>,
	typename Allocator = std::allocator<std::pair<const KeyType, ValueType>>>
using TMap = std::unordered_map<KeyType, ValueType, Hasher, KeyEqual, Allocator>;
