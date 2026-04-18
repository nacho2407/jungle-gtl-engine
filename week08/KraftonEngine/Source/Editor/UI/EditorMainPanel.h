#pragma once

#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Editor/UI/EditorControlPanel.h"
#include "Editor/UI/EditorDetailsPanel.h"
#include "Editor/UI/EditorSceneManagerPanel.h"
#include "Editor/UI/EditorStatPanel.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
  public:
    void Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine);
    void Release();
    void Render(float DeltaTime);
    void Update();
    void HideEditorWindowsForPIE();
    void RestoreEditorWindowsAfterPIE();

  private:
    FWindowsWindow *Window = nullptr;
    UEditorEngine *EditorEngine = nullptr;
    FEditorConsolePanel ConsolePanel;
    FEditorControlPanel ControlPanel;
    FEditorDetailsPanel DetailsPanel;
    FEditorScenePanel ScenePanel;
    FEditorStatPanel StatPanel;
    bool bShowPanelList = false;
    bool bHideEditorWindows = false;
    bool bHasSavedUIVisibility = false;
    bool bSavedShowPanelList = false;
    FEditorSettings::FUIVisibility SavedUIVisibility{};
};
