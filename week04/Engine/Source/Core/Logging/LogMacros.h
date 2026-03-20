#pragma once

#include <cstdio>
#include "LogOutputDevice.h"
#include "Core/CoreGlobals.h"

/**
 * LogOutput Macro
 *
 * Category  : Category name only (e.g. LogEngine, LogRenderer), no registration required
 * Verbosity : Log / Warning / Error
 * fmt, ...  : printf style format string
 *
 * 예시) UE_LOG(LogEngine, Warning, "Value: %d", 42);
 */
#define UE_LOG(Category, ELogVerbosity, fmt, ...)                                                  \
    do                                                                                             \
    {                                                                                              \
        if (GLog)                                                                                  \
        {                                                                                          \
            char _buf[1024];                                                                       \
            snprintf(_buf, sizeof(_buf), "[" #Category "]" fmt, ##__VA_ARGS__);                    \
            GLog->Log(Verbosity, _buf);                                                            \
        }                                                                                          \
    } while (0)