#include "Core/HAL/PlatformMisc.h"

#include <Windows.h>
#include <processthreadsapi.h>
#include <cstdlib>

bool FPlatformMisc::IsDebuggerPresent() { return ::IsDebuggerPresent() != FALSE; }

void FPlatformMisc::DebugBreak() { ::DebugBreak(); }

int32 FPlatformMisc::NumberOfCores()
{
    SYSTEM_INFO SysInfo = {};
    ::GetSystemInfo(&SysInfo);
    return static_cast<int32>(SysInfo.dwNumberOfProcessors);
}

void FPlatformMisc::RequestExit(bool Force)
{
    if (Force)
    {
        ::ExitProcess(0);
    }
    else
    {
        std::exit(0);
    }
}

const char* FPlatformMisc::GetOSName() { return "Windows"; }

const char* FPlatformMisc::GetExecutablePath()
{
    static char Path[MAX_PATH] = {};
    static bool bInitialized = false;

    if (!bInitialized)
    {
        ::GetModuleFileNameA(nullptr, Path, MAX_PATH);
        bInitialized = true;
    }

    return Path;
}