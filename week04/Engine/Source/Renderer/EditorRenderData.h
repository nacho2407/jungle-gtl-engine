#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"
#include "Renderer/Types/EditorShowFlags.h"

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

    EEditorShowFlags ShowFlags =
        EEditorShowFlags::SF_Grid | EEditorShowFlags::SF_WorldAxes | EEditorShowFlags::SF_Gizmo |
        EEditorShowFlags::SF_SelectionOutline | EEditorShowFlags::SF_ObjectLabels;

    FGizmoDrawData Gizmo;
};