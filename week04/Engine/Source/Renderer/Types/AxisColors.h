#pragma once

#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"

static FColor GetAxisBaseColor(EAxis Axis)
{
    switch (Axis)
    {
    case EAxis::X:
        return FColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
    case EAxis::Y:
        return FColor(0.0f, 1.0f, 0.0f, 1.0f); // Green
    case EAxis::Z:
        return FColor(0.0f, 0.5f, 1.0f, 1.0f); // Blue
    case EAxis::Center:
        return FColor(1.0f, 1.0f, 1.0f, 1.0f); // White
    default:
        return FColor(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
}

static FColor GetAxisHighlightColor(EAxis Axis)
{
    switch (Axis)
    {
    case EAxis::X:
    case EAxis::Y:
    case EAxis::Z:
    case EAxis::Center:
        return FColor(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    default:
        return FColor(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
}