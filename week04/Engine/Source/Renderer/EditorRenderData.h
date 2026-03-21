#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"

class FSceneView;

enum class EGizmoType : uint8
{
    None,
    Translation,
    Rotation,
    Scaling,
};

enum class EGizmoHighlight : uint8
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