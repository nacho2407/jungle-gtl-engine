#pragma once
#include <functional>
#include <Windows.h>

using FWndProcFilter = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
using FOnResizeCallback = std::function<void(int Width, int Height)>;