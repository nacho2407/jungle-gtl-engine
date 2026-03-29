#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
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
	FTimer Timer;
};
