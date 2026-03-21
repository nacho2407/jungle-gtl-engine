#pragma once

#include "EditorContext.h"
#include "Core/CoreMinimal.h"

#include "ApplicationCore/Input/InputRouter.h"
#include "ApplicationCore/Input/InputSystem.h"
#include "Input/EditorGlobalContext.h"
#include "Input/GizmoInputContext.h"
#include "Input/ViewPortInputContext.h"
#include "Renderer/Scene.h"

class FPanelManager;

class FEditor
{
public:
    /* Default Functions */
    void Create(HWND);
    void Release();
    
    void Initialize();
    void Tick(Engine::ApplicationCore::FInputSystem * InputSystem);
    
    void OnWindowResized(float Width, float Height);
    void SetMainLoopFPS(float FPS) { CurFPS = FPS; }
    
    /* From Panel */
    void CreateNewScene();
    void ClearScene();
    
private:
    
    
public:
    
    
private:
    /* Input Routing */
    Engine::ApplicationCore::FInputRouter InputRouter;
    
    /* Input Contexts */
    FEditorGlobalContext EditorGlobalContext;
    FViewPortInputContext ViewPortInputContext;
    FGizmoInputContext GizmoInputContext;
    FEditorContext EditorContext;
    
    /* Panel */
    FPanelManager* PanelManager = nullptr;

    /* Gizmo */
    
    /* Scene */
    FScene * CurScene = nullptr;
    
    /* Properties */
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
    
    float CurFPS = 0.0f;    //  Panel에 Display
    
};
