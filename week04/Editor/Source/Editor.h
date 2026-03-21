#pragma once

#include "ApplicationCore/Input/InputRouter.h"
#include "Core/CoreMinimal.h"
#include "Renderer/Scene.h"

class ENGINE_API FEditor
{
public:
    /* 기본 기능 */
    void Create(HWND);
    void Release();

    void BeginPlay();
    void Update();

    void OnWindowResized(float Width, float Height);
    void SetMainLoopFPS(float FPS) { CurFPS = FPS; }
    
    /* From Panel */
    void CreateNewScene();
    void ClearScene();
    
private:


public:


private:
    /* Input System */
    Engine::ApplicationCore::FInputRouter FInputRouter;
    
    /* Panel */
    
    /* Gizmo */

    /* Scene */
    FScene * CurScene = nullptr;
    
    /* Properties */
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;

    float CurFPS = 0.0f;    //  Panel로 바로 넘겨주면, 삭제 가능

};