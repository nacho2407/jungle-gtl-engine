#pragma once

namespace Engine::ApplicationCore
{
    enum class EKey
    {
        Unknown,
        W,
        E,
        R,
        Q,
        F,
        Delete,
        LeftCtrl,
        LeftShift,
        LeftAlt,
        MouseLeft,
        MouseRight,
        MouseMiddle,
        Count
    };

    enum class EInputEventType
    {
        None,
        KeyDown,
        KeyUp,
        MouseButtonDown,
        MouseButtonUp,
        MouseWheel,
        CharInput,
        WindowResize,
        FocusGained,
        FocusLost,
        Quit
    };

    struct FModifierKeysState
    {
        bool bCtrl = false;
        bool bShift = false;
        bool bAlt = false;
    };

    struct FInputEvent
    {
        EInputEventType Type = EInputEventType::None;
        EKey            Key = EKey::Unknown;

        int MouseX = 0;
        int MouseY = 0;
        int WheelDelta = 0;

        wchar_t Character = 0;

        int Width = 0;
        int Height = 0;

        bool               bRepeat = false;
        FModifierKeysState Modifiers;
    };
} // namespace Engine::ApplicationCore
