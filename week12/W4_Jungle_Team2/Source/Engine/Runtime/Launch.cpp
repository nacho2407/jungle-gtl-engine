#include "Engine/Runtime/Launch.h"

#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Core/CrashDump.h"

#if IS_OBJ_VIEWER
#include <iostream>
#include <cstdio>

void CreateDebugConsole()
{
    // 1. 콘솔 창 할당
    AllocConsole();

    // 2. 표준 출력(stdout, stderr) 및 입력(stdin)을 콘솔 창으로 리다이렉션
    FILE* pFile;
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    freopen_s(&pFile, "CONOUT$", "w", stderr);
    freopen_s(&pFile, "CONIN$", "r", stdin);

    // 3. C++ 표준 스트림 상태 초기화
    std::cout.clear();
    std::cin.clear();
    std::cerr.clear();
    
    // 테스트 출력
    std::cout << "========================================" << std::endl;
    std::cout << " Debug Console Initialized Successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
}
#endif

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
#if IS_OBJ_VIEWER
	CreateDebugConsole();
#endif

	__try
	{
		return GuardedMain(hInstance, nShowCmd);
	}
	__except (WriteCrashDump(GetExceptionInformation()))
	{
		return static_cast<int>(GetExceptionCode());
	}
}
