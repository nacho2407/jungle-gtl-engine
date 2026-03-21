#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"

class FEngineLoop
{
public:
	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	void Shutdown();

private:
	void TickFrame();
	void InitializeTiming();
	void CreateEngine();

private:
	FWindowsApplication Application;

	float DeltaTime = 0.0f;

	LARGE_INTEGER Frequency = {};
	LARGE_INTEGER PrevTime = {};
	LARGE_INTEGER CurrTime = {};
};
