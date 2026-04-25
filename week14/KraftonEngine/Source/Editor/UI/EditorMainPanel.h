#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorSceneWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/ContentBrowser/ContentBrowser.h"

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
	void RenderConsoleDrawer(float DeltaTime);
	void RenderFooterOverlay(float DeltaTime);
	void HandleGlobalShortcuts();
	void ToggleConsoleDrawer(bool bFocusInput);

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
	bool bHideEditorWindows = false;
	bool bHasSavedUIVisibility = false;
	bool bSavedShowWidgetList = false;
	bool bConsoleDrawerVisible = false;
	bool bBringConsoleDrawerToFrontNextFrame = false;
	bool bFocusConsoleInputNextFrame = false;
	bool bFocusConsoleButtonNextFrame = false;
	int32 ConsoleBacktickCycleState = 0;
	float ConsoleDrawerAnim = 0.0f;
	FEditorSettings::FUIVisibility SavedUIVisibility{};
};
