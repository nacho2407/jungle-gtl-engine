#pragma once
#include "URender.h"

class USceneComponent;
class Editor;
class UCamera;
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

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND HWnd = nullptr;
	URenderer* Renderer = nullptr;
	Editor* EditorInst = nullptr;
	UCamera* Camera = nullptr;
	UPicker* Picker = nullptr;

};

