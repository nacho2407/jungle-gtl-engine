#include "Renderer/Draw/GizmoDrawer.h"

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"

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

    FPrimitiveRenderItem MakePrimitiveItem(const FMatrix& InWorld, const FColor& InColor,
                                           EBasicMeshType InMeshType)
    {
        FPrimitiveRenderItem Item = {};
        Item.World = InWorld;
        Item.Color = InColor;
        Item.MeshType = InMeshType;
        return Item;
    }

    static constexpr EAxis GizmoAxes[] = {EAxis::X, EAxis::Y, EAxis::Z};

    // 기본 축 메쉬(Cylinder/Cone/Cube 배치)가 로컬 +Z 방향으로 놓여 있다고 가정
    FMatrix MakeAxisBasis(EAxis InAxis)
    {
        switch (InAxis)
        {
        case EAxis::X:
            return FMatrix::MakeFromZ(FVector::ForwardVector); // +Z -> +X
        case EAxis::Y:
            return FMatrix::MakeFromZ(FVector::RightVector); // +Z -> +Y
        case EAxis::Z:
        default:
            return FMatrix::MakeFromZ(FVector::UpVector); // +Z -> +Z
        }
    }

    // Ring 메쉬가 로컬 XY 평면에 있고 법선이 로컬 +Z 라고 가정
    FMatrix MakeRingBasis(EAxis InAxis)
    {
        switch (InAxis)
        {
        case EAxis::X:
            return FMatrix::MakeFromZ(FVector::ForwardVector); // ring normal -> +X
        case EAxis::Y:
            return FMatrix::MakeFromZ(FVector::RightVector); // ring normal -> +Y
        case EAxis::Z:
        default:
            return FMatrix::MakeFromZ(FVector::UpVector); // ring normal -> +Z
        }
    }
} // namespace

void FGizmoDrawer::Draw(FD3D11MeshBatchRenderer& InMeshRenderer,
                        const FEditorRenderData& InEditorRenderData)
{
    if (!IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo) ||
        InEditorRenderData.SceneView == nullptr)
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
        InMeshRenderer.AddPrimitives(GizmoPrimitives);
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
    const FMatrix GizmoFrame = InGizmoDrawData.Frame.GetMatrixWithoutScale();

    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix AxisBasis = MakeAxisBasis(Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftLength));

            // cylinder 메쉬가 원래 z=0~1 이므로 offset 없음
            const FMatrix World = LocalScale * AxisBasis * GizmoFrame;

            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cylinder));
        }

        {
            const FMatrix LocalScale =
                FMatrix::MakeScale(FVector(Style.TranslationHeadRadius, Style.TranslationHeadRadius,
                                           Style.TranslationHeadLength));

            // cone 메쉬도 원래 z=0~1 이므로 shaft 끝에만 붙이면 됨
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.TranslationShaftLength));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoFrame;

            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cone));
        }
    }
}

void FGizmoDrawer::AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                    const FGizmoDrawData&         InGizmoDrawData) const
{
    const FMatrix GizmoFrame = InGizmoDrawData.Frame.GetMatrixWithoutScale();

    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix RingBasis = MakeRingBasis(Axis);

        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));

        const FMatrix World = LocalScale * RingBasis * GizmoFrame;

        OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Ring));
    }
}

void FGizmoDrawer::AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                   const FGizmoDrawData&         InGizmoDrawData) const
{
    const FMatrix GizmoFrame = InGizmoDrawData.Frame.GetMatrixWithoutScale();

    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix AxisBasis = MakeAxisBasis(Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(
                Style.ScalingShaftRadius, Style.ScalingShaftRadius, Style.ScalingShaftLength));

            // cylinder가 원점에서 시작하므로 offset 없음
            const FMatrix World = LocalScale * AxisBasis * GizmoFrame;

            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cylinder));
        }

        {
            const FMatrix LocalScale = FMatrix::MakeScale(
                FVector(Style.ScalingHandleSize, Style.ScalingHandleSize, Style.ScalingHandleSize));

            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoFrame;

            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cube));
        }
    }
}