#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"

enum class EGizmoMode : uint8
{
    None,
    Translate,
    Rotate,
    Scale,
};

enum class EGizmoAxis : uint8
{
    None,
    X,
    Y,
    Z,
    XY,
    YZ,
    ZX,
    XYZ,
};

struct FEditorGizmoState
{
    bool bVisible = false;

    EGizmoMode Mode = EGizmoMode::None;
    EGizmoAxis HotAxis = EGizmoAxis::None;
    EGizmoAxis ActiveAxis = EGizmoAxis::None;

    bool bDragging = false;

    FMatrix Transform;
};