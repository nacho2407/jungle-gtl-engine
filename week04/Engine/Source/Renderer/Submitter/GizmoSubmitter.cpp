#include "Renderer/Submitter/GizmoSubmitter.h"

#include "Renderer/D3D11/D3D11OverlayMeshRenderer.h"
#include "Renderer/D3D11/D3D11ObjectIdRenderer.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/PickId.h"

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

    FObjectIdRenderItem MakeObjectIdItem(const FMatrix& InWorld, EBasicMeshType InMeshType,
                                         uint32 InObjectId)
    {
        FObjectIdRenderItem Item = {};
        Item.World = InWorld;
        Item.MeshType = InMeshType;
        Item.ObjectId = InObjectId;
        return Item;
    }

    static constexpr EAxis GizmoAxes[] = {EAxis::X, EAxis::Y, EAxis::Z};

    FMatrix MakeAxisBasis(EAxis InAxis)
    {
        switch (InAxis)
        {
        case EAxis::X:
            return FMatrix::MakeFromZ(FVector::ForwardVector);
        case EAxis::Y:
            return FMatrix::MakeFromZ(FVector::RightVector);
        case EAxis::Z:
        default:
            return FMatrix::MakeFromZ(FVector::UpVector);
        }
    }

    FMatrix MakeRingBasis(EAxis InAxis)
    {
        switch (InAxis)
        {
        case EAxis::X:
            return FMatrix::MakeFromZ(FVector::ForwardVector);
        case EAxis::Y:
            return FMatrix::MakeFromZ(FVector::RightVector);
        case EAxis::Z:
        default:
            return FMatrix::MakeFromZ(FVector::UpVector);
        }
    }

    FMatrix BuildGizmoMatrix(const FGizmoDrawData& InGizmoDrawData)
    {
        constexpr float MinGizmoScale = 2.0f;
        constexpr float MaxGizmoScale = 5.0f;

        float SafeScale = InGizmoDrawData.Scale;
        if (SafeScale < MinGizmoScale)
        {
            SafeScale = MinGizmoScale;
        }
        else if (SafeScale > MaxGizmoScale)
        {
            SafeScale = MaxGizmoScale;
        }

        return FMatrix::MakeScale(SafeScale) * InGizmoDrawData.Frame.GetMatrixWithoutScale();
    }
} // namespace

void FGizmoSubmitter::Submit(FD3D11OverlayMeshRenderer& InMeshRenderer,
                             const FEditorRenderData&   InEditorRenderData)
{
    if (!IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo) ||
        InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    const FMatrix GizmoMatrix = BuildGizmoMatrix(InEditorRenderData.Gizmo);

    TArray<FPrimitiveRenderItem> GizmoPrimitives;

    switch (InEditorRenderData.Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        AddTranslationGizmo(GizmoPrimitives, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Rotation:
        AddRotationGizmo(GizmoPrimitives, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Scaling:
        AddScalingGizmo(GizmoPrimitives, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    default:
        break;
    }

    if (!GizmoPrimitives.empty())
    {
        InMeshRenderer.AddPrimitives(GizmoPrimitives);
    }
}

void FGizmoSubmitter::Submit(FD3D11ObjectIdRenderer&  InObjectIdRenderer,
                             const FEditorRenderData& InEditorRenderData) const
{
    if (!IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo) ||
        InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    const FMatrix GizmoMatrix = BuildGizmoMatrix(InEditorRenderData.Gizmo);

    TArray<FObjectIdRenderItem> GizmoItems;

    switch (InEditorRenderData.Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        AddTranslationGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Rotation:
        AddRotationGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Scaling:
        AddScalingGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    default:
        break;
    }

    if (!GizmoItems.empty())
    {
        InObjectIdRenderer.AddPrimitives(GizmoItems);
    }
}

FColor FGizmoSubmitter::ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const
{
    return IsAxisHighlighted(InAxis, InHighlight) ? GetAxisHighlightColor(InAxis)
                                                  : GetAxisBaseColor(InAxis);
}

void FGizmoSubmitter::AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                          const FGizmoDrawData&         InGizmoDrawData,
                                          const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix AxisBasis = MakeAxisBasis(Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftLength));

            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.TranslationShaftLength * 0.5f));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cylinder));
        }

        {
            const FMatrix LocalScale =
                FMatrix::MakeScale(FVector(Style.TranslationHeadRadius, Style.TranslationHeadRadius,
                                           Style.TranslationHeadLength));

            const FMatrix LocalOffset = FMatrix::MakeTranslation(FVector(
                0.0f, 0.0f, Style.TranslationShaftLength + Style.TranslationHeadLength * 0.5f));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cone));
        }
    }
}

void FGizmoSubmitter::AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                       const FGizmoDrawData&         InGizmoDrawData,
                                       const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix RingBasis = MakeRingBasis(Axis);
        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));
        const FMatrix World = LocalScale * RingBasis * InGizmoMatrix;

        OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Ring));
    }
}

void FGizmoSubmitter::AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                      const FGizmoDrawData&         InGizmoDrawData,
                                      const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix AxisBasis = MakeAxisBasis(Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(
                Style.ScalingShaftRadius, Style.ScalingShaftRadius, Style.ScalingShaftLength));

            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength * 0.5f));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cylinder));
        }

        {
            const FMatrix LocalScale = FMatrix::MakeScale(
                FVector(Style.ScalingHandleSize, Style.ScalingHandleSize, Style.ScalingHandleSize));

            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength));

            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Cube));
        }
    }
}

void FGizmoSubmitter::AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                          const FGizmoDrawData&        InGizmoDrawData,
                                          const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FMatrix AxisBasis = MakeAxisBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftRadius,
                                                                  Style.TranslationShaftLength));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.TranslationShaftLength * 0.5f));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cylinder, PickObjectId));
        }

        {
            const FMatrix LocalScale =
                FMatrix::MakeScale(FVector(Style.TranslationHeadRadius, Style.TranslationHeadRadius,
                                           Style.TranslationHeadLength));
            const FMatrix LocalOffset = FMatrix::MakeTranslation(FVector(
                0.0f, 0.0f, Style.TranslationShaftLength + Style.TranslationHeadLength * 0.5f));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cone, PickObjectId));
        }
    }
}

void FGizmoSubmitter::AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                       const FGizmoDrawData&        InGizmoDrawData,
                                       const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FMatrix RingBasis = MakeRingBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);
        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));
        const FMatrix World = LocalScale * RingBasis * InGizmoMatrix;
        OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Ring, PickObjectId));
    }
}

void FGizmoSubmitter::AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                      const FGizmoDrawData&        InGizmoDrawData,
                                      const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxes)
    {
        const FMatrix AxisBasis = MakeAxisBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(
                Style.ScalingShaftRadius, Style.ScalingShaftRadius, Style.ScalingShaftLength));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength * 0.5f));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cylinder, PickObjectId));
        }

        {
            const FMatrix LocalScale = FMatrix::MakeScale(
                FVector(Style.ScalingHandleSize, Style.ScalingHandleSize, Style.ScalingHandleSize));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * InGizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cube, PickObjectId));
        }
    }
}
