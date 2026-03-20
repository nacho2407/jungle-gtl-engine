#pragma once

#include <windows.h>

#include "Editor/EditorEngine.h"

// 엔진의 전체 생명주기(초기화/루프/종료)를 담당하는 루프 클래스
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

private:
	HWND HWindow = nullptr;

	bool bIsExit = false;
	bool bIsResizing = false;

	float DeltaTime = 0.0f;

	LARGE_INTEGER Frequency = {};
	LARGE_INTEGER PrevTime = {};
	LARGE_INTEGER CurrTime = {};

	FEditorEngine Editor;
};
