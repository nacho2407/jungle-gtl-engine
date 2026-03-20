#pragma once

#include "Core/CoreMinimal.h"
#include "Editor.h" //  이후에 Directory 만들어야 하나?

class FEditorEngineLoop
{
public:
    bool PreInit(HINSTANCE HInstance, uint32 NCmdShow);
    int32 Run();
    void Shutdown();
    
private:
    static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    LRESULT WndProc(HWND HWnd, uint32 Message, WPARAM WParam, LPARAM LParam);
    
    void Tick();
    
    void InitializeForTime();
public:
    
private:
    /* Time Measure */
    float DeltaTime = 0.0f;
    float Accumulator = 0.0f;
    float MainLoopFPS = 0.0f;
    
    LARGE_INTEGER Frequency = {};
    LARGE_INTEGER PrevTime = {};
    LARGE_INTEGER CurTime = {};
    
    /* Flags */
    bool bIsExit = false;
    // bool bIsResizing = false;
    
    HWND HWindow = nullptr;
    
    FEditor Editor;
};
