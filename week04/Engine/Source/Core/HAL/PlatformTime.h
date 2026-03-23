#pragma once

#include "Core/HAL/PlatformTypes.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <chrono>
#include <thread>
#endif

struct ENGINE_API FPlatformTime
{
    static double Seconds();
    static uint64 Cycles64();
    static void   Sleep(float Seconds);
};