#pragma once
#include <queue>
#include <Windows.h>
#include "InputTypes.h"
#include "InputState.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputSystem
    {
      public:
        void BeginFrame();
        void ProcessWin32Message(UINT Msg, WPARAM WParam, LPARAM LParam);

        bool               PollEvent(FInputEvent& OutEvent);
        const FInputState& GetState() const { return State; }

      private:
        void UpdateModifiers();

      private:
        FInputState             State;
        std::queue<FInputEvent> EventQueue;
    };
} // namespace Engine::ApplicationCore
