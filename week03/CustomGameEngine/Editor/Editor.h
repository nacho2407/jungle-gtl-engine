#pragma once

#include <windows.h>
#include <d3d11.h>

struct EditorControlPanel;
struct EditorConsole;
struct EditorPropertyPanel;
struct EditorStatPanel;
struct EditorToolbar;
class USceneComponent;
class UCameraComponent;
class URenderer;

class Editor
{
public:
	Editor();
	~Editor();

public:
	bool Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void OnBeforeRender();
	void DrawUI();
	void Finalize();

	void SetRenderer(URenderer* renderer) { Renderer = renderer; }
	void SetCamera(UCameraComponent* camera) { Camera = camera; }
	void SelectComponent(USceneComponent* component);
	USceneComponent* GetSelectedComponent() const { return SelectedComponent; }

private:
	void DrawControlPanel();
	void DrawConsole();
	void DrawPropertyPanel();
	void DrawStatPanel();
	void DrawToolbar();

private:
	EditorControlPanel* ControlPanel = nullptr;
	EditorConsole* Console = nullptr;
	EditorPropertyPanel* PropertyPanel = nullptr;
	EditorStatPanel* StatPanel = nullptr;
	EditorToolbar* Toolbar = nullptr;

	URenderer* Renderer = nullptr;
	UCameraComponent* Camera = nullptr;
	USceneComponent* SelectedComponent = nullptr;
};
