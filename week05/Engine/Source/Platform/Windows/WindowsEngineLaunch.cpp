#include "Platform/Windows/WindowsEngineLaunch.h"

#include "Core/EngineLoop.h"

int FWindowsEngineLaunch::Run(HINSTANCE hInstance, const FEngineLaunchConfig& Config)
{
	// EngineLoop를 만들기 전에 Windows 프로세스 단위 초기화를 먼저 수행한다.
	if (!InitializeProcess())
	{
		Shutdown();
		return -1;
	}

	// EngineLoop가 앱 시작, 엔진 초기화, 프레임 반복 실행을 담당한다.
	FEngineLoop EngineLoop;

	// PreInit은 앱과 메인 윈도우를 만들고 엔진 인스턴스를 준비한다.
	if (!EngineLoop.PreInit(hInstance, Config))
	{
		EngineLoop.Exit();
		Shutdown();
		return -1;
	}

	// Init은 호스트 윈도우 정보를 엔진에 넘기고 런타임 시스템을 초기화한다.
	if (!EngineLoop.Init())
	{
		EngineLoop.Exit();
		Shutdown();
		return -1;
	}

	// EngineLoop가 종료를 요청할 때까지 한 프레임씩 반복 실행한다.
	while (!EngineLoop.IsExitRequested())
	{
		EngineLoop.Tick();
	}

	EngineLoop.Exit();
	Shutdown();

	return 0;
}

bool FWindowsEngineLaunch::InitializeProcess()
{
	// COM은 에디터와 호스트 기능에서 필요하므로 프로세스 시작 시 한 번 초기화한다.
	ComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(ComResult) && ComResult != RPC_E_CHANGED_MODE)
	{
		MessageBox(nullptr, L"CoInitializeEx failed", L"COM Error", MB_OK);
		return false;
	}

	return true;
}

void FWindowsEngineLaunch::Shutdown()
{
	if (SUCCEEDED(ComResult) || ComResult == S_FALSE)
	{
		CoUninitialize();
	}

	ComResult = E_FAIL;
}
