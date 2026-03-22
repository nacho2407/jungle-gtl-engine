#pragma once

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

class FPanelManager;

class FEditor
{
  public:
    /* Default Functions */
    void Create();
    void Release();

    void Initialize();
    void Tick(float DeltaTime, Engine::ApplicationCore::FInputSystem* InputSystem);

    void OnWindowResized(float Width, float Height);
    void SetMainLoopFPS(float FPS) { CurFPS = FPS; }

    /* From Panel */
    void CreateNewScene();
    void ClearScene();

  private:
  public:
  private:
    /* Viewports */
    //FEditorViewport MainViewport;
    FEditorViewportClient ViewportClient;

    /* Input Contexts */
    FEditorContext        EditorContext;

    /* Panel */
    FPanelManager* PanelManager = nullptr;

    /* Gizmo */

    /* Properties */
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;

    float CurFPS = 0.0f; //  Panel에 Display
};
