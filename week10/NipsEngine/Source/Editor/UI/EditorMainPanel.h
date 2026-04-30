#pragma once

#include "ImGui/imgui.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorFooterLogSystem.h"
#include "Editor/UI/EditorMaterialWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorToolbarWidget.h"
#include "Editor/UI/EditorPlayStreamWidget.h"
#include "Editor/Viewport/ViewportLayout.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
struct ID3D11Device;
struct ID3D11ShaderResourceView;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();

	FEditorPropertyWidget& GetPropertyWidget() { return PropertyWidget; }
	FEditorMaterialWidget& GetMaterialWidget() { return MaterialWidget; }
	FEditorSceneWidget& GetSceneWidget() { return SceneWidget; }

	bool RequestNewScene();
	bool RequestLoadSceneWithDialog();
	bool RequestSaveScene();
	bool RequestSaveSceneAsWithDialog();

	void ResetWidgetSelections()
	{
		PropertyWidget.ResetSelection();
		MaterialWidget.ResetSelection();
	}

	enum class EViewportToolIcon : int32
	{
		Menu,
		Select,
		Translate,
		Rotate,
		Scale,
		TranslateSnap,
		RotateSnap,
		ScaleSnap,
		WorldSpace,
		LocalSpace,
		Camera,
		Setting,
		Count,
	};

private:
	void RenderEditorToolbar();
	void RenderDockSpace();
	void RenderViewportHostWindow();
	void RenderViewportMenuBarForIndex(int32 ViewportIndex);
	void RenderViewportIconToolbarForIndex(int32 ViewportIndex);
	bool DrawViewportTextButton(const char* Id, const char* Label, bool bPairFirst = false, bool bPairSecond = false);
	bool DrawViewportIconButton(const char* Id, EViewportToolIcon Icon, const char* FallbackLabel, const char* Tooltip, bool bSelected = false, bool bEnabled = true);
	void LoadViewportToolIcons(ID3D11Device* Device);
	void ReleaseViewportToolIcons();
	void TickViewportContextMenu();
	void RenderViewportContextMenu();
	void RenderConsoleDrawer(float DeltaTime);
	void RenderFooterOverlay(float DeltaTime);
	void UpdateFooterEventLogs();

private:
	struct FViewportContextMenuState
	{
		bool bRightClickTracking = false;
		int32 TrackingViewportIndex = -1;
		float RightClickTravelSq = 0.0f;
		POINT PressScreenPos = { 0, 0 };
		int32 PendingPopupViewportIndex = -1;
		ImVec2 PendingPopupScreenPos = ImVec2(0.0f, 0.0f);
		int32 PendingSpawnViewportIndex = -1;
		float PendingSpawnLocalX = 0.0f;
		float PendingSpawnLocalY = 0.0f;
	};

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;

	ImVector<ImWchar> FontGlyphRanges; // 폰트 아틀라스 빌드 전까지 수명 유지 필요
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorMaterialWidget MaterialWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
	FEditorStatWidget StatWidget;
	FEditorToolbarWidget ToolbarWidget;
	FEditorPlayStreamWidget PlayStreamWidget;

	bool bShowConsole = true;
	bool bShowControl = true;
	bool bShowProperty = true;
	bool bShowSceneManager = true;
	bool bShowMaterialEditor = true;
	bool bShowStatProfiler = true;
	bool bShowPlayStream = true;
	bool bConsoleDrawerVisible = false;
	bool bBringConsoleDrawerToFrontNextFrame = false;
	bool bFocusConsoleInputNextFrame = false;
	bool bFocusConsoleButtonNextFrame = false;
	int32 ConsoleBacktickCycleState = 0;
	float ConsoleDrawerAnim = 0.0f;
	bool bFooterEventStateInitialized = false;
	bool bPrevPIEPlaying = false;
	EViewportPlayState PrevEditorState = EViewportPlayState::Editing;
	FViewportContextMenuState ViewportContextMenuState;
	FEditorFooterLogSystem FooterLogSystem;
	ID3D11ShaderResourceView* ViewportToolIcons[static_cast<int32>(EViewportToolIcon::Count)] = {};
	ID3D11ShaderResourceView* ViewportLayoutIcons[static_cast<int32>(EEditorViewportLayoutMode::Max)] = {};
	ID3D11ShaderResourceView* AddActorIconSRV = nullptr;
};
