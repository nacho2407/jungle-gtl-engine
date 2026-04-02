#pragma once
#include <deque>
#include <queue>

// Synchronization, if needed, should be handled by the caller.
template <typename T, typename Container = std::deque<T>>
using TQueue = std::queue<T, Container>;
