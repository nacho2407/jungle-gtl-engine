#include "Renderer/Submitter/GizmoObjectIdSubmitter.h"

#include "Renderer/Types/PickId.h"

namespace
{
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

    FObjectIdRenderItem MakeObjectIdItem(const FMatrix& InWorld, EBasicMeshType InMeshType,
                                         uint32 InObjectId)
    {
        FObjectIdRenderItem Item = {};
        Item.World = InWorld;
        Item.MeshType = InMeshType;
        Item.ObjectId = InObjectId;
        return Item;
    }

    FMatrix MakeGizmoObjectIdFrame(const FGizmoDrawData& InGizmoDrawData)
    {
        return FMatrix::MakeScale(InGizmoDrawData.Scale) * InGizmoDrawData.Frame.GetMatrixWithoutScale();
    }
} // namespace

void FGizmoObjectIdSubmitter::Submit(TArray<FObjectIdRenderItem>& OutItems,
                                     const FEditorRenderData&     InEditorRenderData) const
{
    if (!IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo) ||
        InEditorRenderData.SceneView == nullptr)
    {
        return;
    }

    switch (InEditorRenderData.Gizmo.GizmoType)
    {
    case EGizmoType::Translation:
        AddTranslationGizmo(OutItems, InEditorRenderData.Gizmo);
        break;
    case EGizmoType::Rotation:
        AddRotationGizmo(OutItems, InEditorRenderData.Gizmo);
        break;
    case EGizmoType::Scaling:
        AddScalingGizmo(OutItems, InEditorRenderData.Gizmo);
        break;
    default:
        break;
    }
}

void FGizmoObjectIdSubmitter::AddTranslationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                                  const FGizmoDrawData&         InGizmoDrawData) const
{
    const FMatrix GizmoMatrix = MakeGizmoObjectIdFrame(InGizmoDrawData);

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
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cylinder, PickObjectId));
        }

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(Style.TranslationHeadRadius,
                                                                  Style.TranslationHeadRadius,
                                                                  Style.TranslationHeadLength));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f,
                                                 Style.TranslationShaftLength +
                                                     Style.TranslationHeadLength * 0.5f));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cone, PickObjectId));
        }
    }
}

void FGizmoObjectIdSubmitter::AddRotationGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                               const FGizmoDrawData&         InGizmoDrawData) const
{
    const FMatrix GizmoMatrix = MakeGizmoObjectIdFrame(InGizmoDrawData);

    for (EAxis Axis : GizmoAxes)
    {
        const FMatrix RingBasis = MakeRingBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);
        const FMatrix LocalScale = FMatrix::MakeScale(
            FVector(Style.RotationRingRadius, Style.RotationRingRadius, Style.RotationRingRadius));
        const FMatrix World = LocalScale * RingBasis * GizmoMatrix;
        OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Ring, PickObjectId));
    }
}

void FGizmoObjectIdSubmitter::AddScalingGizmo(TArray<FObjectIdRenderItem>& OutItems,
                                              const FGizmoDrawData&         InGizmoDrawData) const
{
    const FMatrix GizmoMatrix = MakeGizmoObjectIdFrame(InGizmoDrawData);

    for (EAxis Axis : GizmoAxes)
    {
        const FMatrix AxisBasis = MakeAxisBasis(Axis);
        const uint32  PickObjectId = PickId::MakeGizmoPartId(InGizmoDrawData.GizmoType, Axis);

        {
            const FMatrix LocalScale = FMatrix::MakeScale(FVector(Style.ScalingShaftRadius,
                                                                  Style.ScalingShaftRadius,
                                                                  Style.ScalingShaftLength));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength * 0.5f));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cylinder, PickObjectId));
        }

        {
            const FMatrix LocalScale = FMatrix::MakeScale(
                FVector(Style.ScalingHandleSize, Style.ScalingHandleSize, Style.ScalingHandleSize));
            const FMatrix LocalOffset =
                FMatrix::MakeTranslation(FVector(0.0f, 0.0f, Style.ScalingShaftLength));
            const FMatrix World = LocalScale * LocalOffset * AxisBasis * GizmoMatrix;
            OutItems.push_back(MakeObjectIdItem(World, EBasicMeshType::Cube, PickObjectId));
        }
    }
}
