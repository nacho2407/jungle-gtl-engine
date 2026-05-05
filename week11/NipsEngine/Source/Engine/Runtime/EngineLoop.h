#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Runtime/WindowsFileWatcher.h"
#include "Engine/Runtime/GameSplashScreen.h"
#include "Core/Logging/Timer.h"

class FEngineLoop
{
public:
	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	void Shutdown();

private:
	void CreateEngine();

private:
	FWindowsApplication Application;
	FWindowsFileWatcher ShaderDirectoryWatcher;
	FGameSplashScreen GameSplashScreen;
	FTimer Timer;
	bool bComInitialized = false;
};
