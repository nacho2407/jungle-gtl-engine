#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>
#include "ImGui/imgui.h"
#include "UI/EditorConsole.h"
#include "UI/EditorControlPanel.h"
#include "UI/EditorPropertyPanel.h"
#include "UI/EditorStatPanel.h"
#include "UI/EditorToolbar.h"
#include "UI/EditorSceneManagerPanel.h"
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
class UBillboardTextComponent;

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

	/*-----picking 관련 함수들-----*/
	/**
	 * 현재 선택된 컴포넌트들을 모두 선택 해제합니다.
	 * Editor의 SelectedComponents 배열과 각 컴포넌트의 bIsSelected를 업데이트합니다.
	 */
	void ClearSelection();
	/**
	 * picking에 걸린 오브젝트를 입력으로 받아 처리합니다.
	 * Editor의 SelectedComponent와 걸린 오브젝트의 bIsSelected를 업데이트합니다.
	 *
	 * \param component
	 */
	void SelectSingleComponent(USceneComponent* component);
	/**
	 * picking에 걸린 오브젝트를 입력으로 받아 처리합니다.
	 * 다중 picking이며 오브젝트가 배열에 추가되는 방식이고, 허공을 클릭해도 기존 picking들은 유지됩니다.
	 * Editor의 SelectedComponents와 걸린 오브젝트의 bIsSelected를 업데이트합니다.
	 *
	 * \param component
	 */
	void SelectMultiComponent(USceneComponent* component);
	const TArray<USceneComponent*>& GetSelectedComponents() const { return SelectedComponents; }
	USceneComponent* GetLastSelectedComponent() const;
	bool IsComponentSelected(USceneComponent* component) const;

	void LoadSettings();
	void SaveSettings();

private:
	/**
	 * @brief 선택 대상에 연결된 billboard text component를 찾습니다.
	 *
	 * @param component 검색할 대상 scene component
	 * @return 연결된 billboard text component, 없으면 nullptr
	 */
	UBillboardTextComponent* FindBillboardTextComponent(USceneComponent* component) const;

	/**
	 * @brief 선택된 scene component용 billboard text component를 생성하거나 재사용합니다.
	 *
	 * @param component 선택된 대상 scene component
	 */
	void EnsureBillboardTextForSelection(USceneComponent* component);

	/**
	 * @brief 선택 해제된 scene component용 billboard text component를 제거합니다.
	 *
	 * @param component 선택 해제된 대상 scene component
	 */
	void RemoveBillboardTextForSelection(USceneComponent* component);

	void DrawDockSpace();
	void SetupDefaultDockLayout(ImGuiID dockspaceID);
	void DrawControlPanel();
	void DrawConsole();
	void DrawPropertyPanel();
	void DrawStatPanel();
	void DrawToolbar();
	void DrawSceneManagerPanel();
	void UpdateGizmoSelection();

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
	//USceneComponent* SelectedComponent = nullptr;
	TArray<USceneComponent*> SelectedComponents;
	UGridComp* Grid = nullptr;
	bool bDockLayoutInitialized = false;
	bool bUseDefaultDockLayout = true;
	std::string ImGuiIniPath;
};
