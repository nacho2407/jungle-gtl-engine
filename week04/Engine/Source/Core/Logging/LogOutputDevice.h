#pragma once

enum class ELogVerbosity
{
    Log,
    Warning,
    Error
};

class FLogOutputDevice
{
  public:
    virtual ~FLogOutputDevice() = default;
    virtual void Log(ELogVerbosity Verbosity, const char *Message) = 0;
};