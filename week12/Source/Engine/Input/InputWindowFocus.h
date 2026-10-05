#pragma once

#include <windows.h>

namespace InputWindowFocus
{
inline bool IsForegroundWindowOwnedByCurrentProcess()
{
    HWND ForegroundWindow = ::GetForegroundWindow();
    if (!ForegroundWindow)
    {
        return false;
    }

    DWORD ForegroundProcessId = 0;
    ::GetWindowThreadProcessId(ForegroundWindow, &ForegroundProcessId);
    return ForegroundProcessId == ::GetCurrentProcessId();
}
}
