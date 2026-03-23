#pragma once

#include "Chrome/EditorChrome.h"
#include "EditorContext.h"
#include "Menu/EditorMenuRegistry.h"
#include "Core/CoreMinimal.h"

#include "ApplicationCore/Input/InputRouter.h"
#include "ApplicationCore/Input/InputSystem.h"
#include "Input/EditorGlobalContext.h"
#include "Input/GizmoInputContext.h"
#include "Input/NavigationInputContext.h"
#include "Engine/Scene.h"
#include "Logging/EditorLogBuffer.h"
#include "Logging/EditorLogEntry.h"
#include "Viewport/EditorViewport.h"
#include "Viewport/EditorViewportClient.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/SceneView.h"

class FPanelManager;
struct FPanelDescriptor;
class UObject;

class FEditor
{
  public:
    void Create();
    void Release();

    void Initialize();
    void Tick(float DeltaTime, Engine::ApplicationCore::FInputSystem* InputSystem);
    void SetChromeHost(IEditorChromeHost* InChromeHost);

    void OnWindowResized(float Width, float Height);
    void SetMainLoopFPS(float FPS)
    {
        CurFPS = FPS;
        EditorContext.CurrentFPS = FPS;
    }

    void CreateNewScene();
    void ClearScene();
    void SetSelectedObject(UObject* InSelectedObject);
    UObject* GetSelectedObject() const { return EditorContext.SelectedObject; }

    const FEditorRenderData& GetEditorRenderData() const { return EditorRenderData; }
    const FSceneRenderData& GetSceneRenderData() const { return SceneRenderData; }
    FEditorViewportClient& GetViewportClient() { return ViewportClient; }
    const FEditorViewportClient& GetViewportClient() const { return ViewportClient; }

    void DrawPanel();

  private:
    void BuildRenderData();
    void BuildSceneView();
    // 커스텀 타이틀바 아래 전체 영역을 도킹 가능한 루트 dockspace로 사용합니다.
    void DrawRootDockSpace();
    void DrawAboutPopup();
    void RequestAboutPopup();
    void RegisterDefaultCommands();
    void RegisterDefaultMenus();
    void RegisterWindowPanelCommand(const FPanelDescriptor& Descriptor);

  private:
    FEditorViewportClient ViewportClient;
    FEditorContext EditorContext;
    FPanelManager* PanelManager = nullptr;
    FEditorChrome EditorChrome;
    FEditorMenuRegistry MenuRegistry;
    IEditorChromeHost* ChromeHost = nullptr;

    FEditorRenderData EditorRenderData;
    FSceneRenderData SceneRenderData;
    FSceneView SceneView;

    FScene* CurScene = nullptr;
    
    /* Logging */
    FEditorLogBuffer LogBuffer;

    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
    float CurFPS = 0.0f;
    bool bRequestOpenAboutPopup = false;
    bool bAboutPopupOpen = false;
};
