// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
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

// FEditorMainPanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorMainPanel
{
public:
    void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
    void Release();
    void Render(float DeltaTime);
    void Update();
    void HideEditorWindowsForPIE();
    void RestoreEditorWindowsAfterPIE();

private:
    FWindowsWindow* Window = nullptr;
    UEditorEngine* EditorEngine = nullptr;
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
