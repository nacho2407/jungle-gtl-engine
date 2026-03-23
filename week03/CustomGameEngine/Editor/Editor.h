#pragma once

#include <windows.h>
#include <d3d11.h>
#include "ImGui/imgui.h"
#include "UI/EditorConsole.h"
#include "UI/EditorControlPanel.h"
#include "UI/EditorPropertyPanel.h"
#include "UI/EditorStatPanel.h"
#include "UI/EditorToolbar.h"
#include "UI/EditorSceneManagerPanel.h"
#include "UI/ImGuiLayout.h"
#include "Gizmo.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "EngineCore/ResourceManager.h"
#include "Component/GridComp.h"

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

	void SetRenderer(URenderer* renderer);
	URenderer* GetRenderer() const { return Renderer; }
	void SetCamera(UCameraComponent* camera) { Camera = camera; }
	void SetGrid(UGridComp* grid);
	void SelectComponent(USceneComponent* component);
	USceneComponent* GetSelectedComponent() const { return SelectedComponent; }

	void LoadSettings();
	void SaveSettings();

private:
	void DrawDockSpace();
	void SetupDefaultDockLayout(ImGuiID dockspaceID);
	void DrawControlPanel(ImGuiLayout&);
	void DrawConsole(ImGuiLayout&);
	void DrawPropertyPanel(ImGuiLayout&);
	void DrawStatPanel(ImGuiLayout&);
	void DrawToolbar(ImGuiLayout&);
	void DrawSceneManagerPanel(ImGuiLayout&);

private:
	//panel
	EditorControlPanel* ControlPanel = nullptr;
	EditorConsole* Console = nullptr;
	EditorPropertyPanel* PropertyPanel = nullptr;
	EditorStatPanel* StatPanel = nullptr;
	EditorToolbar* Toolbar = nullptr;
	EditorSceneManagerPanel* SceneMgrPanel = nullptr;

	ImGuiLayout UILayout;

	URenderer* Renderer = nullptr;
	UCameraComponent* Camera = nullptr;
	USceneComponent* SelectedComponent = nullptr;
	UGridComp* Grid = nullptr;
	bool bDockLayoutInitialized = false;
};
