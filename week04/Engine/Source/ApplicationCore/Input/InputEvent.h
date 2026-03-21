#pragma once

enum class EKey
{
    Invalid,
    W,
    A,
    S,
    D,
    Q,
    E,
    F,
    L,
    LeftShift,
    SpaceBar,

    LeftMouseButton,
    RightMouseButton,
    MiddleMouseButton,
    MouseWheelAxis
};

enum class EInputEvent
{
    Pressed,
    Released,
    Repeat,
    DoubleClick,
    Axis
};