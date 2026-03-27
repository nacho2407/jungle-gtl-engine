#pragma once

#include "Math/Vector.h"

struct FCursorOverlayState
{
    float ScreenX = 0.0f;
    float ScreenY = 0.0f;

    float CurrentRadius = 0.0f;
    float TargetRadius = 0.0f;
    float MaxRadius = 24.0f;
    float LerpSpeed = 12.0f;

    FVector4 Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);

    bool bPressed = false;
    bool bVisible = false;
};

