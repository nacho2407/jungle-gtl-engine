#pragma once

#include "Core/Math/Color.h"

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
    default:
        return FColor(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
}

static FColor GetAxisHighlightColor(EAxis Axis)
{
    switch (Axis)
    {
    case EAxis::X:
        return FColor(1.0f, 0.4f, 0.4f, 1.0f); // Light Red
    case EAxis::Y:
        return FColor(0.4f, 1.0f, 0.4f, 1.0f); // Light Green
    case EAxis::Z:
        return FColor(0.4f, 0.6f, 1.0f, 1.0f); // Light Blue
    default:
        return FColor(1.0f, 1.0f, 1.0f, 1.0f); // White
    }
}
