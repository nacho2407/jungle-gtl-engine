#include "Renderer/Submitter/OverlayMeshSubmitter.h"

#include "Renderer/D3D11/D3D11MeshBatchRenderer.h"
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
        case EGizmoHighlight::Center:
            return InAxis == EAxis::Center;
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
                                         uint32         InObjectId)
    {
        FObjectIdRenderItem Item = {};
        Item.World = InWorld;
        Item.MeshType = InMeshType;
        Item.ObjectId = InObjectId;
        return Item;
    }

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
        constexpr float MinGizmoScale = 0.1f;
        constexpr float MaxGizmoScale = 1000.0f;

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

void FOverlayMeshSubmitter::Submit(FD3D11MeshBatchRenderer& InMeshRenderer,
                                  const FEditorRenderData&   InEditorRenderData) const
{
    if (!InEditorRenderData.bShowGizmo || InEditorRenderData.SceneView == nullptr ||
        InEditorRenderData.Gizmo.GizmoType == EGizmoType::None)
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

void FOverlayMeshSubmitter::SubmitCenterHandle(FD3D11MeshBatchRenderer& InMeshRenderer,
                                               const FEditorRenderData&   InEditorRenderData) const
{
    if (!InEditorRenderData.bShowGizmo || InEditorRenderData.SceneView == nullptr ||
        InEditorRenderData.Gizmo.GizmoType == EGizmoType::None)
    {
        return;
    }

    if (InEditorRenderData.Gizmo.GizmoType != EGizmoType::Translation &&
        InEditorRenderData.Gizmo.GizmoType != EGizmoType::Scaling)
    {
        return;
    }

    const FMatrix GizmoMatrix = BuildGizmoMatrix(InEditorRenderData.Gizmo);

    TArray<FPrimitiveRenderItem> CenterPrimitives;
    AddCenterHandle(CenterPrimitives, InEditorRenderData.Gizmo, GizmoMatrix);

    if (!CenterPrimitives.empty())
    {
        InMeshRenderer.AddPrimitives(CenterPrimitives);
    }
}

void FOverlayMeshSubmitter::Submit(FD3D11ObjectIdRenderer&  InObjectIdRenderer,
                                  const FEditorRenderData& InEditorRenderData) const
{
    if (!InEditorRenderData.bShowGizmo || InEditorRenderData.SceneView == nullptr ||
        InEditorRenderData.Gizmo.GizmoType == EGizmoType::None)
    {
        return;
    }

    const FMatrix GizmoMatrix = BuildGizmoMatrix(InEditorRenderData.Gizmo);

    TArray<FObjectIdRenderItem> GizmoItems;

    switch (InEditorRenderData.Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        AddTranslationGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        AddCenterHandle(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Rotation:
        AddRotationGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    case EGizmoType::Scaling:
        AddScalingGizmo(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        AddCenterHandle(GizmoItems, InEditorRenderData.Gizmo, GizmoMatrix);
        break;
    default:
        break;
    }

    if (!GizmoItems.empty())
    {
        InObjectIdRenderer.AddPrimitives(GizmoItems);
    }
}

FColor FOverlayMeshSubmitter::ResolveAxisColor(EAxis InAxis, EGizmoHighlight InHighlight) const
{
    return IsAxisHighlighted(InAxis, InHighlight)
               ? GetAxisHighlightColor(InAxis)
               : GetAxisBaseColor(InAxis);
}

void FOverlayMeshSubmitter::AddCenterHandle(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                      const FGizmoDrawData&         InGizmoDrawData,
                                      const FMatrix&                InGizmoMatrix) const
{
    if (InGizmoDrawData.GizmoType == EGizmoType::None)
    {
        return;
    }

    const FMatrix LocalScale = FMatrix::MakeScale(
        FVector(Style.CenterHandleRadius, Style.CenterHandleRadius, Style.CenterHandleRadius));
    const FMatrix World = LocalScale * InGizmoMatrix;
    OutPrimitives.push_back(MakePrimitiveItem(
        World, ResolveAxisColor(EAxis::Center, InGizmoDrawData.Highlight), EBasicMeshType::Sphere));
}

void FOverlayMeshSubmitter::AddCenterHandle(TArray<FObjectIdRenderItem>& OutItems,
                                      const FGizmoDrawData&        InGizmoDrawData,
                                      const FMatrix&               InGizmoMatrix) const
{
    const uint32 CenterId = PickId::MakeGizmoCenterId(InGizmoDrawData.GizmoType);
    if (CenterId == PickId::None)
    {
        return;
    }

    const FMatrix LocalScale = FMatrix::MakeScale(
        FVector(Style.CenterHandleRadius, Style.CenterHandleRadius, Style.CenterHandleRadius));
    const FMatrix World = LocalScale * InGizmoMatrix;
    OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Sphere, CenterId));
}

void FOverlayMeshSubmitter::AddTranslationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                          const FGizmoDrawData&         InGizmoDrawData,
                                          const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
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

void FOverlayMeshSubmitter::AddRotationGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                       const FGizmoDrawData&         InGizmoDrawData,
                                       const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
    {
        const FColor  AxisColor = ResolveAxisColor(Axis, InGizmoDrawData.Highlight);
        const FMatrix RingBasis = MakeRingBasis(Axis);
        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));
        const FMatrix World = LocalScale * RingBasis * InGizmoMatrix;

        OutPrimitives.push_back(MakePrimitiveItem(World, AxisColor, EBasicMeshType::Ring));
    }
}

void FOverlayMeshSubmitter::AddScalingGizmo(TArray<FPrimitiveRenderItem>& OutPrimitives,
                                      const FGizmoDrawData&         InGizmoDrawData,
                                      const FMatrix&                InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
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

void FOverlayMeshSubmitter::AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                          const FGizmoDrawData&        InGizmoDrawData,
                                          const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
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

void FOverlayMeshSubmitter::AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                       const FGizmoDrawData&        InGizmoDrawData,
                                       const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
    {
        const FMatrix RingBasis = MakeRingBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);
        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));
        const FMatrix World = LocalScale * RingBasis * InGizmoMatrix;
        OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Ring, PickObjectId));
    }
}

void FOverlayMeshSubmitter::AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                      const FGizmoDrawData&        InGizmoDrawData,
                                      const FMatrix&               InGizmoMatrix) const
{
    for (EAxis Axis : GizmoAxisOrder)
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