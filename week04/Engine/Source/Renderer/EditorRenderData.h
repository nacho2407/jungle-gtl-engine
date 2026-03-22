#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"

class FSceneView;

enum class EGizmoType : uint8
{
    None = 0,
    Translation = 1,
    Rotation = 2,
    Scaling = 3,
    Count = 4
};

enum class EGizmoHighlight : uint8
{
    None = 0,
    X = 1,
    Y = 2,
    Z = 3,
    XY = 4,
    YZ = 5,
    ZX = 6,
    XYZ = 7,
};

struct FGizmoDrawData
{
    bool bVisible = false;

    EGizmoType      GizmoType = EGizmoType::None;
    EGizmoHighlight Highlight = EGizmoHighlight::None;
    FMatrix         Transform;
};

struct FEditorRenderData
{
    const FSceneView* SceneView = nullptr;
    bool              bShowGrid = true;
    bool              bShowWorldAxes = true;
    bool              bShowGizmo = true;
    bool              bShowSelectionOutline = true;
    bool              bShowObjectLabels = true;
    FGizmoDrawData    Gizmo;
};