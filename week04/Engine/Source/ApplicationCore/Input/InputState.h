#pragma once

#pragma once
#include "InputTypes.h"

namespace Engine::ApplicationCore
{
    struct FInputState
    {
        bool bKeysDown[static_cast<int32>(EKey::Count)] = {};

        int32 MouseX = 0;
        int32 MouseY = 0;
        int32 MouseDeltaX = 0;
        int32 MouseDeltaY = 0;
        int32 WheelDelta = 0;

        FModifierKeysState Modifiers;

        void BeginFrame()
        {
            MouseDeltaX = 0;
            MouseDeltaY = 0;
            WheelDelta = 0;
        }

        bool IsKeyDown(EKey Key) const { return bKeysDown[static_cast<int32>(Key)]; }
    };
} // namespace Engine::ApplicationCore
