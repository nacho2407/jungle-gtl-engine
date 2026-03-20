#pragma once

#include "Core/CoreMinimal.h"

class FEditor
{
public:
    void Create(HWND);
    void Release();
    
    void BeginPlay();
    void Update();
    
    void BeginFrame();
    void EndFrame();
    
private:
    
    
public:
    
    
private:
    /* Panel */
    
    /* Gizmo */
    
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
    
    
};