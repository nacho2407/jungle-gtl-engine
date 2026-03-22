#include "Renderer/EditorDraw/GizmoDrawer.h"

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/Types/AxisColors.h"

// TODO: 기즈모 축 회전

namespace
{
    bool IsAxisHighlighted(EAxis InAxis, EGizmoHighlight InHighlight)
    {
        switch (InHighlight)
        {
        case EGizmoHighlight::X:
            return InAxis == EAxis::X;
        case EGizmoHighlight::Y:
            return InAxis == EAxis::Y;
        case EGizmoHighlight::Z:
            return InAxis == EAxis::Z;
        case EGizmoHighlight::XY:
            return InAxis == EAxis::X || InAxis == EAxis::Y;
        case EGizmoHighlight::YZ:
            return InAxis == EAxis::Y || InAxis == EAxis::Z;
        case EGizmoHighlight::ZX:
            return InAxis == EAxis::Z || InAxis == EAxis::X;
        case EGizmoHighlight::XYZ:
            return true;
        default:
            return false;
        }
    }
} // namespace

void FGizmoDrawer::Draw(FD3D11MeshBatchRenderer& InMeshRenderer,
                        const FEditorRenderData& InEditorRenderData)
{
    if (!InEditorRenderData.bShowGizmo || InEditorRenderData.SceneView == nullptr ||
        !InEditorRenderData.Gizmo.bVisible)
    {
        return;
    }

    TArray<FPrimitiveRenderItem> GizmoPrimitives;

    switch (InEditorRenderData.Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        AddTranslationGizmo(GizmoPrimitives, InEditorRenderData.Gizmo);
        break;
    case EGizmoType::Rotation:
        AddRotationGizmo(GizmoPrimitives, InEditorRenderData.Gizmo);
        break;
    case EGizmoType::Scaling:
        AddScalingGizmo(GizmoPrimitives, InEditorRenderData.Gizmo);
        break;
    default:
        break;
    }

    if (!GizmoPrimitives.empty())
    {
        InMeshRenderer.Render({InEditorRenderData.SceneView, ViewMode,
                               ESceneShowFlags::SF_Primitives, bUseInstancing, GizmoPrimitives});
    }
}

FColor FGizmoDrawer::ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const
{
    return IsAxisHighlighted(InAxis, InHighlight) ? GetAxisHighlightColor(InAxis)
                                                  : GetAxisBaseColor(InAxis);
}

void FGizmoDrawer::AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                       const FGizmoDrawData&         InGizmoDrawData) const
{
    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const EAxis  Axis = static_cast<EAxis>(AxisIndex);
        const FColor AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);

        FPrimitiveRenderItem ShaftItem = {};
        ShaftItem.World = InGizmoDrawData.Transform;
        ShaftItem.Color = AxisColor;
        ShaftItem.MeshType = EBasicMeshType::Cylinder;
        OutPrimitives.push_back(ShaftItem);

        FPrimitiveRenderItem HeadItem = {};
        HeadItem.World = InGizmoDrawData.Transform;
        HeadItem.Color = AxisColor;
        HeadItem.MeshType = EBasicMeshType::Cone;
        OutPrimitives.push_back(HeadItem);
    }
}

void FGizmoDrawer::AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                    const FGizmoDrawData&         InGizmoDrawData) const
{
    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const EAxis  Axis = static_cast<EAxis>(AxisIndex);
        const FColor AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);

        FPrimitiveRenderItem RingItem = {};
        RingItem.World = InGizmoDrawData.Transform;
        RingItem.Color = AxisColor;
        RingItem.MeshType = EBasicMeshType::Ring;
        OutPrimitives.push_back(RingItem);
    }
}

void FGizmoDrawer::AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                   const FGizmoDrawData&         InGizmoDrawData) const
{
    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const EAxis  Axis = static_cast<EAxis>(AxisIndex);
        const FColor AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);

        FPrimitiveRenderItem ShaftItem = {};
        ShaftItem.World = InGizmoDrawData.Transform;
        ShaftItem.Color = AxisColor;
        ShaftItem.MeshType = EBasicMeshType::Cylinder;
        OutPrimitives.push_back(ShaftItem);

        FPrimitiveRenderItem HandleItem = {};
        HandleItem.World = InGizmoDrawData.Transform;
        HandleItem.Color = AxisColor;
        HandleItem.MeshType = EBasicMeshType::Cube;
        OutPrimitives.push_back(HandleItem);
    }
}
