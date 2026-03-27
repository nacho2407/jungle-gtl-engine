#include "Platform/Windows/WindowsEngineLaunch.h"

#include "Core/EngineLoop.h"

int FWindowsEngineLaunch::Run(HINSTANCE hInstance, const FEngineLaunchConfig& Config)
{
	if (!InitializeProcess())
	{
		Shutdown();
		return -1;
	}

	FEngineLoop EngineLoop;

	if (!EngineLoop.PreInit(hInstance, Config))
	{
		EngineLoop.Exit();
		Shutdown();
		return -1;
	}

	if (!EngineLoop.Init())
	{
		EngineLoop.Exit();
		Shutdown();
		return -1;
	}

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
