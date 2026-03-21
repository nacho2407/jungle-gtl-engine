#pragma once

#include "Core/CoreMinimal.h"
#include "Editor.h" //  이후에 Directory 만들어야 하나?
#include "Launch/EngineLoop.h"

class FEditorEngineLoop : public IEngineLoop
{
public:
    bool PreInit(HINSTANCE HInstance, uint32 NCmdShow) override;
    int32 Run() override;
    void ShutDown() override;
    
private:
    static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    LRESULT WndProc(HWND HWnd, uint32 Message, WPARAM WParam, LPARAM LParam);
    
    void Tick() override;
    
    void InitializeForTime() override;
public:
    
private:
    /* Time Measure */
    float DeltaTime = 0.0f;
    float MainLoopFPS = 0.0f;

    float PrevTime = 0.0f;
    
    /* Flags */
    bool bIsExit = false;
    
    /* Properties */
    HWND HWindow = nullptr;
    
    /* Editor */
    FEditor * Editor = nullptr;
    
    /* Engine */
    //  FEngine * Engine = nullptr;
};
