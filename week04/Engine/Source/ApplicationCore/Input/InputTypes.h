#pragma once

#include "Core/CoreMinimal.h"

namespace Engine::ApplicationCore
{
    enum class EKey
    {
        Unknown,
        W,
        A,
        S,
        D,
        E,
        Q,
        X,
        Delete,
        //LeftCtrl,
        //LeftShift,
        //LeftAlt,
        MouseLeft,
        MouseRight,
        MouseMiddle,
        Count,
        Space
    };

    enum class EInputEventType
    {
        None,
        KeyDown,
        KeyUp,
        MouseButtonDown,
        MouseButtonUp,
        MouseMove,
        MouseWheel,
        //CharInput,
    };

    struct FModifierKeysState
    {
        bool bCtrlDown = false;
        bool bShiftDown = false;
        bool bAltDown = false;
    };

    struct FInputEvent
    {
        EInputEventType Type = EInputEventType::None;
        EKey            Key = EKey::Unknown;

        int32 MouseX = 0;
        int32 MouseY = 0;
        int32 WheelDelta = 0;

        wchar_t Character = 0;

        int32 Width = 0;
        int32 Height = 0;

        bool               bRepeat = false;
        FModifierKeysState Modifiers;
    };
} // namespace Engine::ApplicationCore
