#pragma once
#include <windows.h>
#include <d3d11.h>

class EditorControlPanel;
class EditorConsole;
class EditorPropertyPanel;
class USceneComponent;

class Editor
{
public:
	Editor();
	~Editor();

public:
	bool Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void Render();
	void Finalize();

private:
	void DrawControlPanel();
	void DrawConsole();
	void DrawPropertyPanel();

private:
	EditorControlPanel* ControlPanel;
	EditorConsole* Console;
	EditorPropertyPanel* PropertyPanel;

	USceneComponent* SelectedComponent = nullptr;
};
