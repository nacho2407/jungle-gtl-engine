#pragma once
#include "OutlinerWindow.h" 
#include "ControlPanelWindow.h"
#include "PropertyWindow.h"
#include "ConsoleWindow.h"
#include "StatWindow.h"
#include "Types/ObjectPtr.h"
#include "ContentBrowserWindow.h"
#include "Viewport/ViewportTypes.h"
#include "EditorDebugState.h"

class FEditorEngine;
class FWindowsWindow;
class FRenderer;
class AActor;
class FEditorUI
{
public:
	void Initialize(FEditorEngine* InEngine);
	void SetupWindow(FWindowsWindow* InWindow);
	void InitializeRendererResources(FRenderer* InRenderer);
	void ShutdownRendererResources(FRenderer* InRenderer);
	void BeginFrame();
	void EndFrame();
	void Render();
	void OnSlateReady();
	void SyncSelectedActorProperty();
	bool GetViewportMousePosition(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const;
	bool HasHostWindow() const { return MainWindow != nullptr; }
	FWindowsWindow* GetHostWindow() const { return MainWindow; }

	FConsoleWindow& GetConsole() { return Console; }
	FEditorEngine* GetEngine() { return Engine; }

	bool GetCentralDockRect(FRect& OutRect) const;
	void SaveEditorSettings();
	const FDebugState& GetDebugState() const { return DebugState; }

private:
	bool InitializeImGui(FRenderer* InRenderer);
	void ShutdownImGui();
	void BuildDefaultLayout(uint32 DockID);
	void LoadEditorSettings();
	std::wstring GetEditorIniPathW() const;
	FEditorEngine* Engine = nullptr;
	TObjectPtr<AActor> CachedSelectedActor;

	FWindowsWindow* MainWindow = nullptr;

	FControlPanelWindow ControlPanel;
	FPropertyWindow Property;
	FConsoleWindow Console;
	FStatWindow Stat;
	FOutlinerWindow Outliner;
	FContentBrowserWindow ContentBrowser;

	bool bWindowSetup = false;
	bool bViewportClientActive = false;
	bool bLayoutInitialized = false;
	bool bImGuiInitialized = false;
	bool bRequestViewportFocusOnNextRender = false;
	FRect CentralDockRect;
	bool bHasCentralDockRect = false;
	FDebugState DebugState;
};
