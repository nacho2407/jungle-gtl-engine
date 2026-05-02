#pragma once

#include "Core/CoreTypes.h"
#include "UI/RuntimeUITypes.h"

enum class ERuntimeUIInputEventType : uint8
{
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel,
    KeyDown,
    KeyUp,
    TextInput
};

enum class ERuntimeUIMouseButton : uint8
{
    None,
    Left,
    Right,
    Middle
};

struct FRuntimeUIInputEvent
{
    ERuntimeUIInputEventType Type = ERuntimeUIInputEventType::MouseMove;
    ERuntimeUIMouseButton MouseButton = ERuntimeUIMouseButton::None;
    FRuntimeUIVector2 ScreenPosition;
    float WheelDelta = 0.0f;
    uint32 KeyCode = 0;
    uint32 Character = 0;
};
