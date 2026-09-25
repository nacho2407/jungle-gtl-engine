#pragma once
#pragma once

#include "Core/CoreMinimal.h"
#include "InputTypes.h"

namespace Engine::ApplicationCore
{
    struct ENGINE_API FInputState
    {
        //  TODO : 추후에 Size enumerator 추가하여 생성해야 함 - uint32로 바꿔야하나?
        bool KeysDown[static_cast<int32>(EKey::Count)] = {};
        
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
        
        bool IsKeyDown(EKey Key) const
        {
            return KeysDown[static_cast<int32>(Key)];
        }
    };
}