// 런타임 영역의 세부 동작을 구현합니다.
#include "Engine/Runtime/Launch.h"

#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Platform/CrashDump.h"

namespace
{
int GuardedMain(HINSTANCE hInstance, int nShowCmd)
{
    FEngineLoop EngineLoop;
    if (!EngineLoop.Init(hInstance, nShowCmd))
    {
        return -1;
    }

    const int ExitCode = EngineLoop.Run();
    EngineLoop.Shutdown();
    return ExitCode;
}
} // namespace

int Launch(HINSTANCE hInstance, int nShowCmd)
{
    // __try
    {
        return GuardedMain(hInstance, nShowCmd);
    }
    //__except (WriteCrashDump(GetExceptionInformation()))
    //{
    //	return static_cast<int>(GetExceptionCode());
    //}
}
