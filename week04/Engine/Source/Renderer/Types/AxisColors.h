#pragma once

#include "Core/Math/Vector4.h"

enum class EAxis : uint8
{
    X = 0,
    Y,
    Z
};

static FVector4 GetAxisBaseColor(EAxis Axis)
{
    switch (Axis)
    {
    case EAxis::X:
        return FVector4(1.0f, 0.0f, 0.0f, 1.0f);
    case EAxis::Y:
        return FVector4(0.0f, 1.0f, 0.0f, 1.0f);
    case EAxis::Z:
        return FVector4(0.0f, 0.5f, 1.0f, 1.0f);
    default:
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

static FVector4 GetAxisHighlightColor(EAxis Axis)
{
    switch (Axis)
    {
    case EAxis::X:
        return FVector4(1.0f, 0.4f, 0.4f, 1.0f);
    case EAxis::Y:
        return FVector4(0.4f, 1.0f, 0.4f, 1.0f);
    case EAxis::Z:
        return FVector4(0.4f, 0.6f, 1.0f, 1.0f);
    default:
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}