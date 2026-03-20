#pragma once

#include "HAL/PlatformTypes.h"

class FPlatformMisc
{
  public:
    static bool IsDebuggerPresent();
    static void DebugBreak();

    static int32       NumberOfCores();
    static void        RequestExit(bool Force);
    static const char *GetOSName();
    static const char *GetExecutablePath();
};
