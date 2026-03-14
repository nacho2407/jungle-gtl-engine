#pragma once
#include <windows.h>
#include <d3d11.h>

struct EditorControlPanel;
struct EditorConsole;
struct EditorPropertyPanel;
struct EditorStatPanel;
class USceneComponent;
class UCamera;
class URenderer;

class Editor
{
public:
	Editor();
	~Editor();

public:
	bool Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void Render();
	void DrawUI();
	void Finalize();

	void SetRenderer(URenderer* renderer) { Renderer = renderer; }
	void SetCamera(UCamera* camera) { Camera = camera; }
	void SelectComponent(USceneComponent* component) { SelectedComponent = component; }
	void GetSelectedComponent(USceneComponent*& outComponent) { outComponent = SelectedComponent; }

private:
	void DrawControlPanel();
	void DrawConsole();
	void DrawPropertyPanel();
	void DrawStatPanel();

private:
	EditorControlPanel* ControlPanel = nullptr;
	EditorConsole* Console = nullptr;
	EditorPropertyPanel* PropertyPanel = nullptr;
	EditorStatPanel* StatPanel = nullptr;

	URenderer* Renderer = nullptr;
	UCamera* Camera = nullptr;
	USceneComponent* SelectedComponent = nullptr;
};
