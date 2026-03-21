#pragma once

#include <windows.h>

#include "Engine/Runtime/Engine.h"

class FEngineLoop
{
public:
	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	void Shutdown();

private:
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam);

	void TickFrame();
	void InitializeTiming();
	void CreateEngine();

private:
	HWND HWindow = nullptr;

	bool bIsExit = false;
	bool bIsResizing = false;

	float DeltaTime = 0.0f;

	LARGE_INTEGER Frequency = {};
	LARGE_INTEGER PrevTime = {};
	LARGE_INTEGER CurrTime = {};
};
