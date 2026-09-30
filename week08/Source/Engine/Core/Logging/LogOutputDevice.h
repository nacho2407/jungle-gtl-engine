#pragma once

#include "Core/CoreTypes.h"

enum class ELogLevel : uint8
{
    Verbose = 0,
    Debug,
    Info,
    Warning,
    Error
};

const char* GetLogLevelLabel(ELogLevel Level);
ELogLevel   GetGlobalLogLevel();
void        SetGlobalLogLevel(ELogLevel Level);
bool        ShouldLog(ELogLevel Level);

class ILogOutputDevice
{
public:
    virtual ~ILogOutputDevice() = default;
    virtual void Log(ELogLevel Level, const char* Category, const char* Message, const char* FormattedMessage) = 0;
};
