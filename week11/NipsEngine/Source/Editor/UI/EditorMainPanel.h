#pragma once

#include "ImGui/imgui.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorContentBrowserWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorFooterLogSystem.h"
#include "Editor/UI/EditorMaterialWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorRuntimeUIPreviewWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorViewportOverlayWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorToolbarWidget.h"
#include "Editor/UI/EditorPlayStreamWidget.h"
#include "Editor/Packaging/GamePackager.h"
#include "Editor/Viewport/ViewportLayout.h"

#include <future>

class FRenderer;
class UEditorEngine;
class UMaterialInterface;
class UPrimitiveComponent;
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
	FEditorControlWidget& GetControlWidget() { return ControlWidget; }

	bool RequestNewScene();
	bool RequestLoadSceneWithDialog();
	bool RequestSaveScene();
	bool RequestSaveSceneAsWithDialog();
	void RequestBuildGame();
	void HideEditorWindowsForPIE();
	void RestoreEditorWindowsAfterPIE();
	bool IsPIEViewportFullscreenEnabled() const { return bPIEViewportFullscreenEnabled; }
	void SetPIEViewportFullscreenEnabled(bool bEnabled);
	void OpenMaterialAsset(UMaterialInterface* Material);
	void OpenMaterialSlot(UPrimitiveComponent* PrimitiveComp, int32 SlotIndex);
	void PushFooterLog(const FString& Message);
	void RequestPIEViewportInputFocus();
	bool SpawnPrefabAtOrigin(const FString& PayloadPath);
	bool CanCloseEditor();

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
	FViewportRect GetPIEFixedAspectViewportRect(const FViewportRect& SourceRect) const;
	void ApplyPIEFixedAspectViewportRect();
	void RenderRuntimeUIForPIEViewport(const FViewportRect& ViewportRect, float DeltaTime);
	void RenderViewportMenuBarForIndex(int32 ViewportIndex);
	void RenderViewportIconToolbarForIndex(int32 ViewportIndex);
	bool SpawnStaticMeshFromContentPath(const FString& PayloadPath, int32 ViewportIndex, float LocalX, float LocalY);
	bool SpawnPrefabFromContentPath(const FString& PayloadPath, int32 ViewportIndex, float LocalX, float LocalY);
	void HandleContentBrowserViewportDrop();
	bool DrawViewportTextButton(const char* Id, const char* Label, bool bPairFirst = false, bool bPairSecond = false);
	bool DrawViewportIconButton(const char* Id, EViewportToolIcon Icon, const char* FallbackLabel, const char* Tooltip, bool bSelected = false, bool bEnabled = true, bool bPairFirst = false, bool bPairSecond = false);
	void LoadViewportToolIcons(ID3D11Device* Device);
	void ReleaseViewportToolIcons();
	void TickViewportContextMenu();
	void RenderViewportContextMenu();
	void RenderConsoleDrawer(float DeltaTime);
	void RenderFooterOverlay(float DeltaTime);
	void RenderEditorDebugPanel(float DeltaTime);
	void RenderUndoHistoryPanel(float DeltaTime);
	void RenderBuildGameModal();
	void TickBuildGameTask();
	void UpdateFooterEventLogs();
	void OpenConsoleDrawer(bool bFocusInput = true);
	void CloseConsoleDrawer();
	void OpenContentBrowser();
	void CloseContentBrowser();
	void ToggleContentBrowser();

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

	struct FPIEPanelVisibilitySnapshot
	{
		bool bShowConsole = true;
		bool bShowControl = true;
		bool bShowProperty = true;
		bool bShowSceneManager = true;
		bool bShowMaterialEditor = true;
		bool bShowStatProfiler = true;
		bool bShowPlayStream = true;
		bool bShowEditorDebug = false;
		bool bShowContentBrowser = false;
		bool bConsoleDrawerVisible = false;
		bool bViewportSettingsVisible = false;
		bool bGroupedStatOverlayVisible = false;
	};

	struct FPIEViewportLayoutSnapshot
	{
		bool bValid = false;
		EEditorViewportLayoutMode LayoutMode = EEditorViewportLayoutMode::FourPanes2x2;
		int32 SingleViewportIndex = 0;
		int32 LastFocusedViewportIndex = 0;
	};

	void ApplyPIEViewportFullscreen();
	void RestorePIEViewportLayout();

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;

	ImVector<ImWchar> FontGlyphRanges; // 폰트 아틀라스 빌드 전까지 수명 유지 필요
	FEditorConsoleWidget ConsoleWidget;
	FEditorContentBrowserWidget ContentBrowserWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorMaterialWidget MaterialWidget;
	FEditorViewportOverlayWidget ViewportOverlayWidget;
	FEditorStatWidget StatWidget;
	FEditorToolbarWidget ToolbarWidget;
	FEditorPlayStreamWidget PlayStreamWidget;
	FEditorRuntimeUIPreviewWidget RuntimeUIPreviewWidget;

	bool bShowConsole = true;
	bool bShowControl = false;
	bool bShowProperty = true;
	bool bShowSceneManager = true;
	bool bShowMaterialEditor = true;
	bool bShowStatProfiler = false;
	bool bShowPlayStream = true;
	bool bShowEditorDebug = false;
	bool bShowContentBrowser = false;
	bool bShowUndoHistory = false;
	bool bShowRuntimeUIPreview = false;
	bool bOpenBuildGameModal = false;
	bool bBuildGameInProgress = false;
	FGameBuildSettings PendingBuildSettings;
	std::future<FGamePackageResult> BuildGameFuture;
	char BuildGameNameBuffer[128] = "NipsGame";
	char BuildStartupSceneBuffer[MAX_PATH] = "Asset/Scene/Default.scene";
	char BuildSceneListAddBuffer[MAX_PATH] = "";
	char BuildPlayerControllerClassBuffer[128] = "APlayerController";
	char BuildOutputDirectoryBuffer[MAX_PATH] = "Builds/Windows/NipsGame";
	char BuildIconPathBuffer[MAX_PATH] = "";
	char BuildSplashImagePathBuffer[MAX_PATH] = "";
	int32 DebugGridPrimitiveType = 1;
	int32 DebugGridRows = 4;
	int32 DebugGridCols = 4;
	int32 DebugGridLayers = 1;
	float DebugGridSpacing = 2.0f;
	bool bDebugGridCenter = true;
	FVector DebugGridOrigin = FVector(0.0f, 0.0f, 0.0f);
	bool bConsoleDrawerVisible = false;
	bool bBringConsoleDrawerToFrontNextFrame = false;
	bool bFocusConsoleInputNextFrame = false;
	bool bFocusConsoleButtonNextFrame = false;
	int32 ConsoleBacktickCycleState = 0;
	int32 PendingPIEViewportInputFocusFrames = 0;
	float ConsoleDrawerAnim = 0.0f;
	bool bFooterEventStateInitialized = false;
	bool bPrevPIEPlaying = false;
	EViewportPlayState PrevEditorState = EViewportPlayState::Editing;
	bool bHideEditorWindowsForPIE = false;
	bool bHasSavedPIEPanelVisibility = false;
	bool bPIEViewportFullscreenEnabled = true;
	bool bPIEUseFixedUILayout = true;
	int32 PIEUILayoutWidth = 1920;
	int32 PIEUILayoutHeight = 1080;
	FPIEPanelVisibilitySnapshot SavedPIEPanelVisibility;
	FPIEViewportLayoutSnapshot SavedPIEViewportLayout;
	FViewportContextMenuState ViewportContextMenuState;
	FEditorFooterLogSystem FooterLogSystem;
	TArray<FRuntimeUIRenderContext> PendingPIERmlUiRenderContexts;
	ID3D11ShaderResourceView* ViewportToolIcons[static_cast<int32>(EViewportToolIcon::Count)] = {};
	ID3D11ShaderResourceView* ViewportLayoutIcons[static_cast<int32>(EEditorViewportLayoutMode::Max)] = {};
	ID3D11ShaderResourceView* SaveIconSRV = nullptr;
    ID3D11ShaderResourceView* HotReloadIconSRV = nullptr;
	ID3D11ShaderResourceView* AddActorIconSRV = nullptr;
};
