#pragma once

#include <cstdarg>
#include <cstdio>

#include "Core/CoreGlobals.h"
#include "Core/Logging/LogOutputDevice.h"

#ifndef UE_DEFAULT_LOG_LEVEL
#define UE_DEFAULT_LOG_LEVEL ELogLevel::Verbose
#endif

inline void InitializeDefaultLogLevel()
{
    static bool bInitialized = false;
    if (!bInitialized)
    {
        SetGlobalLogLevel(UE_DEFAULT_LOG_LEVEL);
        bInitialized = true;
    }
}

inline void LogMessage(ELogLevel Level, const char* Category, const char* Format, ...)
{
    InitializeDefaultLogLevel();

    if (!ShouldLog(Level))
    {
        return;
    }

    char Message[2048];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Message, sizeof(Message), Format, Args);
    va_end(Args);

    char FormattedMessage[2304];
    snprintf(FormattedMessage, sizeof(FormattedMessage), "[%s] %s: %s",
             GetLogLevelLabel(Level),
             Category != nullptr ? Category : "Log",
             Message);

    if (GLog)
    {
        GLog->Log(Level, Category, Message, FormattedMessage);
    }
}

#define UE_LOG(Category, Level, Format, ...) \
    do                                       \
    {                                        \
        LogMessage(ELogLevel::Level, #Category, Format, ##__VA_ARGS__); \
    } while (0)
