#pragma once

#include "Core/CoreMinimal.h"
#include "InputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputSystem
    {
      public:
        constexpr FInputSystem() = default;
        ~FInputSystem() = default;

        void BeginFrame();
        LRESULT ProcessWin32Message(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam);
        
        bool PollEvent(FInputEvent & OutEvent);
        const FInputState & GetInputState() const { return State; }
        
    private:
        // static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
        
        void UpdateModifiers();
        
    private:
        FInputState State;
        //  TODO : Build 에러 방지
        TQueue<FInputEvent> EventQueue;
    };    
}

