#pragma once

#include "Math/Vector.h"

struct FFogParams
{
    float    Density           = 0.02f;
    float    HeightFalloff     = 0.2f;
    float    StartDistance     = 0.0f;
    float    CutoffDistance    = 0.0f;
    float    MaxOpacity        = 1.0f;
    FVector4 InscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
