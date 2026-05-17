#include "Engine/Runtime/Launch.h"

#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Core/CrashDump.h"

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
}

int Launch(HINSTANCE hInstance, int nShowCmd)
{
	FCrashHandler::Initialize();

	__try
	{
		return GuardedMain(hInstance, nShowCmd);
	}
	__except (FCrashHandler::HandleException(GetExceptionInformation()))
	{
		return static_cast<int>(GetExceptionCode());
	}
}
