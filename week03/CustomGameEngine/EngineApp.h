#pragma once

#include <windows.h>
#include <functional>
#include "EngineTypes.h"
class URenderer;
class Editor;
class UCameraComponent;
class UPicker;
class EngineApp
{
public:
	EngineApp();
	~EngineApp();

public:
	bool Initialize(HINSTANCE hInstance);
	void Run();
	void Finalize();

private:
	void CreateEngineWindow();
	void ReleaseEngineWindow();
	void ProcessOnResizeCallback(int width, int height);

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND HWnd = nullptr;
	URenderer* Renderer = nullptr;
	Editor* EditorInst = nullptr;
	UPicker* Picker = nullptr;
	TArray<std::function<void(int width, int height)>> OnResizeCallback;
};

