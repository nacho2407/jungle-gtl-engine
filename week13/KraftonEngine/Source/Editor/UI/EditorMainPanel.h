#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/ContentBrowser/ContentBrowser.h"
#include "Math/Vector.h"

class AActor;
class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();
	void SaveToSettings() const;
	void HideEditorWindowsForPIE();
	void RestoreEditorWindowsAfterPIE();

private:
	void RenderMainMenuBar();
	void RenderShortcutOverlay();
	void RenderEditorDebugPanel();
	void RenderConsoleDrawer(float DeltaTime);
	void RenderFooterOverlay(float DeltaTime);
	void HandleGlobalShortcuts();
	void ToggleConsoleDrawer(bool bFocusInput);
	void ProcessPendingDebugActions();

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorSceneWidget SceneWidget;
	FEditorStatWidget StatWidget;
	FEditorContentBrowserWidget ContentBrowserWidget;
	bool bShowWidgetList = false;
	bool bShowShortcutOverlay = false;
	bool bShowEditorDebugPanel = false;
	bool bHideEditorWindows = false;
	bool bHasSavedUIVisibility = false;
	bool bSavedShowWidgetList = false;
	bool bConsoleDrawerVisible = false;
	bool bBringConsoleDrawerToFrontNextFrame = false;
	bool bFocusConsoleInputNextFrame = false;
	bool bFocusConsoleButtonNextFrame = false;
	int32 ConsoleBacktickCycleState = 0;
	float ConsoleDrawerAnim = 0.0f;
	int32 DebugPlaceActorTypeIndex = 0;
	int32 DebugGridRows = 10;
	int32 DebugGridCols = 10;
	int32 DebugGridLayers = 1;
	float DebugGridSpacing = 2.0f;
	bool bDebugGridCenter = true;
	bool bDebugUseCameraOrigin = true;
	float DebugCameraForwardDistance = 30.0f;
	FVector DebugManualGridOrigin = FVector(0.0f, 0.0f, 0.0f);
	bool bDebugRandomYaw = false;
	float DebugRandomYawRange = 180.0f;
	bool bDebugApplyJitter = false;
	float DebugJitterXY = 0.0f;
	float DebugJitterZ = 0.0f;
	TArray<AActor*> DebugLastSpawnedActors;
	bool bPendingClearLastBatch = false;
	FEditorSettings::FUIVisibility SavedUIVisibility{};
};
