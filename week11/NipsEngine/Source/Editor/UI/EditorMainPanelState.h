#pragma once

#include "Core/Containers/Array.h"
#include "Editor/EditorUtils.h"
#include "Editor/Packaging/GamePackager.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Math/Vector.h"
#include "UI/RuntimeUITypes.h"

#include "ImGui/imgui.h"

#include <Windows.h>
#include <future>

class FEditorMainPanel;
struct ID3D11ShaderResourceView;

enum class EEditorMainPanelViewportToolIcon : int32
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

struct FEditorMainPanelViewportContextMenuState
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

struct FEditorMainPanelPIEPanelVisibilitySnapshot
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
	bool bShowProjectSettings = false;
	bool bShowWorldSettings = false;
	bool bConsoleDrawerVisible = false;
	bool bViewportSettingsVisible = false;
	bool bGroupedStatOverlayVisible = false;
};

struct FEditorMainPanelVisibilityState
{
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
	bool bShowProjectSettings = false;
	bool bShowWorldSettings = false;
};

struct FEditorMainPanelPIEViewportLayoutSnapshot
{
	bool bValid = false;
	EEditorViewportLayoutMode LayoutMode = EEditorViewportLayoutMode::FourPanes2x2;
	int32 SingleViewportIndex = 0;
	int32 LastFocusedViewportIndex = 0;
};

struct FEditorMainPanelPendingRuntimeUIDraw
{
	FEditorMainPanel* Owner = nullptr;
	FRuntimeUIRenderContext Context;
};

struct FEditorMainPanelRuntimeUIDrawCallbackState
{
	TArray<FEditorMainPanelPendingRuntimeUIDraw*> PendingCallbacks;
};

struct FEditorMainPanelViewportIconResources
{
	ID3D11ShaderResourceView* ToolIcons[static_cast<int32>(EEditorMainPanelViewportToolIcon::Count)] = {};
	ID3D11ShaderResourceView* LayoutIcons[static_cast<int32>(EEditorViewportLayoutMode::Max)] = {};
	ID3D11ShaderResourceView* SaveIcon = nullptr;
	ID3D11ShaderResourceView* AddActorIcon = nullptr;
};

struct FEditorMainPanelBuildGameModalState
{
	bool bOpenModal = false;
	bool bInProgress = false;
	FGameBuildSettings PendingSettings;
	std::future<FGamePackageResult> Future;
	char GameNameBuffer[128] = "NipsGame";
	char StartupSceneBuffer[MAX_PATH] = "Asset/Scene/Default.scene";
	char SceneListAddBuffer[MAX_PATH] = "";
	char PlayerControllerClassBuffer[128] = "APlayerController";
	char DefaultPawnClassBuffer[128] = "ADefaultPawn";
	char DefaultPawnPrefabPathBuffer[MAX_PATH] = "";
	char OutputDirectoryBuffer[MAX_PATH] = "Builds/Windows/NipsGame";
	char IconPathBuffer[MAX_PATH] = "";
	char SplashImagePathBuffer[MAX_PATH] = "";
};

struct FEditorMainPanelGameModeSettingsState
{
	bool bLoaded = false;
	char PlayerControllerClassBuffer[128] = "APlayerController";
	char DefaultPawnClassBuffer[128] = "ADefaultPawn";
	char DefaultPawnPrefabPathBuffer[MAX_PATH] = "";
	bool bSceneOverrideGameMode = false;
	char ScenePlayerControllerClassBuffer[128] = "APlayerController";
	char SceneDefaultPawnClassBuffer[128] = "ADefaultPawn";
	char SceneDefaultPawnPrefabPathBuffer[MAX_PATH] = "";
};

struct FEditorMainPanelConsoleDrawerState
{
	bool bDrawerVisible = false;
	bool bBringDrawerToFrontNextFrame = false;
	bool bFocusInputNextFrame = false;
	bool bFocusButtonNextFrame = false;
	int32 BacktickCycleState = 0;
	float DrawerAnim = 0.0f;
};

struct FEditorMainPanelDebugGridState
{
	int32 PrimitiveType = 1;
	int32 Rows = 4;
	int32 Cols = 4;
	int32 Layers = 1;
	float Spacing = 2.0f;
	bool bCenter = true;
	FVector Origin = FVector(0.0f, 0.0f, 0.0f);
};

struct FEditorMainPanelFooterEventState
{
	bool bInitialized = false;
	bool bPrevPIEPlaying = false;
	EViewportPlayState PrevEditorState = EViewportPlayState::Editing;
};

struct FEditorMainPanelPIEViewportPresentationState
{
	int32 PendingInputFocusFrames = 0;
	bool bHideEditorWindows = false;
	bool bHasSavedPanelVisibility = false;
	bool bFullscreenEnabled = true;
	bool bUseFixedUILayout = true;
	int32 UILayoutWidth = 1920;
	int32 UILayoutHeight = 1080;
	FEditorMainPanelPIEPanelVisibilitySnapshot SavedPanelVisibility;
	FEditorMainPanelPIEViewportLayoutSnapshot SavedLayout;
};
