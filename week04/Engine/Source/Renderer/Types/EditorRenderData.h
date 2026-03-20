#pragma once

#include "HAL/PlatformTypes.h"
#include "Math/Matrix.h"

class FSceneView;

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

    bool bDrawTranslate = false;
    bool bDrawRotate = false;
    bool bDrawScale = false;

    EGizmoHighlight Highlight = EGizmoHighlight::None;
    FMatrix         Transform;
};

struct FEditorRenderData
{
    const FSceneView *SceneView = nullptr;

    bool bShowGrid = true;
    bool bShowWorldAxes = true;
    bool bShowGizmo = true;
    bool bShowSelectionOutline = true;
    bool bShowObjectLabels = true;

    FGizmoDrawData Gizmo;
};