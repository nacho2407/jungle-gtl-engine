#pragma once
#include <forward_list>
#include <list>
#include <memory>

template <typename T, typename Allocator = std::allocator<T>>
using TLinkedList = std::forward_list<T, Allocator>;

template <typename T, typename Allocator = std::allocator<T>>
using TDoubleLinkedList = std::list<T, Allocator>;
