#pragma once

#include "Core/CoreTypes.h"

enum class EViewMode : int32
{
    Lit = 0,
    Unlit,
    Wireframe,
    Depth,
    Count
};

struct FShowFlags
{
    bool bPrimitives = true;
    bool bGrid = true;
    bool bAxis = true;
    bool bGizmo = true;
    bool bBillboardText = false;
    bool bBoundingVolume = false;
    bool bBVHBoundingVolume = false;
    bool bEnableLOD = true;
    bool bDecals = true;
};
