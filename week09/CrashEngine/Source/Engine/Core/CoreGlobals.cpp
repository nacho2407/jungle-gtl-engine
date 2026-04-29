#include "Core/CoreGlobals.h"

ILogOutputDevice* GLog = nullptr;
ELogLevel         GMinimumLogLevel = ELogLevel::Verbose;

const char* GetLogLevelLabel(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Verbose:
        return "VERBOSE";
    case ELogLevel::Debug:
        return "DEBUG";
    case ELogLevel::Info:
        return "INFO";
    case ELogLevel::Warning:
        return "WARNING";
    case ELogLevel::Error:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

ELogLevel GetGlobalLogLevel()
{
    return GMinimumLogLevel;
}

void SetGlobalLogLevel(ELogLevel Level)
{
    GMinimumLogLevel = Level;
}

bool ShouldLog(ELogLevel Level)
{
    return static_cast<uint8>(Level) >= static_cast<uint8>(GMinimumLogLevel);
}
