#pragma once

#include "UI/RuntimeUITypes.h"

struct FRuntimeUIStyle
{
    FRuntimeUIColor Tint = FRuntimeUIColor(1.0f, 1.0f, 1.0f, 1.0f);
    FRuntimeUIColor BackgroundColor = FRuntimeUIColor(0.0f, 0.0f, 0.0f, 0.35f);
    FRuntimeUIColor TextColor = FRuntimeUIColor(1.0f, 1.0f, 1.0f, 1.0f);
    FRuntimeUIColor DisabledTint = FRuntimeUIColor(0.45f, 0.45f, 0.45f, 0.75f);

    float Alpha = 1.0f;
    float Rounding = 0.0f;
    float FontScale = 1.0f;
};
