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
    Center = 8,
};

struct FGizmoDrawData
{
    EGizmoType      GizmoType = EGizmoType::None;
    EGizmoHighlight Highlight = EGizmoHighlight::None;

    FMatrix Frame = FMatrix::Identity; // Position, Rotation
    float   Scale = 1.f;
};

struct FEditorRenderData
{
    const FSceneView* SceneView = nullptr;

    bool bShowGrid = false;
    bool bShowWorldAxes = false;
    bool bShowSelectionOutline = false;
    bool bEnableOutlineOcclusion = true;
    bool bShowGizmo = false;

    FGizmoDrawData Gizmo;
};
