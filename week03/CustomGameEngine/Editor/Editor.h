#pragma once

#include <windows.h>
#include <d3d11.h>

struct EditorControlPanel;
struct EditorConsole;
struct EditorPropertyPanel;
struct EditorStatPanel;
struct EditorToolbar;
struct EditorSceneManagerPanel;
class USceneComponent;
class UCameraComponent;
class URenderer;
class UGridComp;

class Editor
{
public:
	Editor();
	~Editor();

public:
	bool Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void DrawUI();
	void Finalize();

	void SetRenderer(URenderer* renderer) { Renderer = renderer; }
	void SetCamera(UCameraComponent* camera) { Camera = camera; }
	void SetGrid(UGridComp* grid) { Grid = grid; }
	void SelectComponent(USceneComponent* component);
	USceneComponent* GetSelectedComponent() const { return SelectedComponent; }

private:
	void DrawControlPanel();
	void DrawConsole();
	void DrawPropertyPanel();
	void DrawStatPanel();
	void DrawToolbar();
	void DrawSceneManagerPanel();

private:
	//panel
	EditorControlPanel* ControlPanel = nullptr;
	EditorConsole* Console = nullptr;
	EditorPropertyPanel* PropertyPanel = nullptr;
	EditorStatPanel* StatPanel = nullptr;
	EditorToolbar* Toolbar = nullptr;
	EditorSceneManagerPanel* SceneMgrPanel = nullptr;

	URenderer* Renderer = nullptr;
	UCameraComponent* Camera = nullptr;
	USceneComponent* SelectedComponent = nullptr;
	UGridComp* Grid = nullptr;
};
