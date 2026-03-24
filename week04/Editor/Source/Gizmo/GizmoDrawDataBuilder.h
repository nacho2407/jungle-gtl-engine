#pragma once

#include "Gizmo/EditorGizmoTypes.h"
#include "Renderer/EditorRenderData.h"

inline EGizmoHighlight ToGizmoHighlight(EGizmoAxis InAxis)
{
    switch (InAxis)
    {
    case EGizmoAxis::X:
        return EGizmoHighlight::X;
    case EGizmoAxis::Y:
        return EGizmoHighlight::Y;
    case EGizmoAxis::Z:
        return EGizmoHighlight::Z;
    case EGizmoAxis::XY:
        return EGizmoHighlight::XY;
    case EGizmoAxis::YZ:
        return EGizmoHighlight::YZ;
    case EGizmoAxis::ZX:
        return EGizmoHighlight::ZX;
    case EGizmoAxis::XYZ:
        return EGizmoHighlight::XYZ;
    case EGizmoAxis::None:
    default:
        return EGizmoHighlight::None;
    }
}

inline FGizmoDrawData BGizmoDrawData(const FEditorGizmoState &InState)
{
    FGizmoDrawData Out;
    // Out.bVisible = InState.bVisible;
    // Out.Transform = InState.Transform;

    switch (InState.Mode)
    {
    case EGizmoMode::Translate:
        Out.GizmoType = EGizmoType::Translation;
        break;

    case EGizmoMode::Rotate:
        Out.GizmoType = EGizmoType::Rotation;
        break;

    case EGizmoMode::Scale:
        Out.GizmoType = EGizmoType::Scaling;
        break;

    case EGizmoMode::None:
    default:
        break;
    }

    const EGizmoAxis AxisToShow =
        (InState.ActiveAxis != EGizmoAxis::None) ? InState.ActiveAxis : InState.HotAxis;

    Out.Highlight = ToGizmoHighlight(AxisToShow);

    return Out;
}