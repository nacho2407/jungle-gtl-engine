#pragma once

#include "Chrome/EditorChrome.h"
#include "EditorContext.h"
#include "Core/CoreMinimal.h"

#include "ApplicationCore/Input/InputRouter.h"
#include "ApplicationCore/Input/InputSystem.h"
#include "Input/EditorGlobalContext.h"
#include "Input/GizmoInputContext.h"
#include "Input/ViewPortInputContext.h"
#include "Engine/Scene.h"
#include "Viewport/EditorViewport.h"
#include "Viewport/EditorViewportClient.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/SceneView.h"

class FPanelManager;

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

    const FEditorRenderData& GetEditorRenderData() const { return EditorRenderData; }
    const FSceneRenderData& GetSceneRenderData() const { return SceneRenderData; }

    void DrawPanel();

  private:
    void BuildRenderData();
    void BuildSceneView();

  private:
    FEditorViewportClient ViewportClient;
    FEditorContext EditorContext;
    FPanelManager* PanelManager = nullptr;
    FEditorChrome EditorChrome;

    FEditorRenderData EditorRenderData;
    FSceneRenderData SceneRenderData;
    FSceneView SceneView;

    FScene* CurScene = nullptr;

    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
    float CurFPS = 0.0f;
};
