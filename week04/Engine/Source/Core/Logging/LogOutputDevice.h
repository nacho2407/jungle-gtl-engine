#pragma once
#pragma once

#include "Core/EngineAPI.h"

enum class ELogVerbosity
{
    Log,
    Warning,
    Error
};

class ENGINE_API ILogOutputDevice
{
  public:
    virtual ~ILogOutputDevice() = default;
    virtual void Log(ELogVerbosity Verbosity, const char *Message) = 0;
};