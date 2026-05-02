#include "UUIDTextRenderComponent.h"

#include <cmath>

#include "Object/ObjectFactory.h"

namespace
{
FMatrix BuildStableUUIDTextBillboardMatrix(const FVector& CameraForward, const FVector& WorldLocation, const FVector& WorldScale)
{
    FVector Forward = (CameraForward * -1.0f).Normalized();
    FVector WorldUp(0.0f, 0.0f, 1.0f);

    if (std::abs(Forward.Dot(WorldUp)) > 0.95f)
    {
        WorldUp = FVector(0.0f, 1.0f, 0.0f);
    }

    FVector Right = Forward.Cross(WorldUp);
    if (Right.Dot(Right) < 1.0e-4f)
    {
        Right = FVector(1.0f, 0.0f, 0.0f);
    }
    Right = Right.Normalized();

    const FVector Up = Right.Cross(Forward).Normalized();

    FMatrix RotMatrix;
    RotMatrix.SetAxes(Forward, Right, Up);
    return FMatrix::MakeScaleMatrix(WorldScale) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);
}
} // namespace

IMPLEMENT_HIDDEN_COMPONENT_CLASS(UUUIDTextRenderComponent, UTextRenderComponent)

void UUUIDTextRenderComponent::UpdateWorldMatrix() const
{
    if (!bTransformDirty)
    {
        return;
    }

    const FMatrix RelativeMatrix = GetRelativeMatrix();
    const USceneComponent* Parent = GetParent();
    CachedWorldMatrix = Parent
                            ? RelativeMatrix * FMatrix::MakeTranslationMatrix(Parent->GetWorldLocation())
                            : RelativeMatrix;
    bTransformDirty = false;
}

FMatrix UUUIDTextRenderComponent::CalculateBillboardWorldMatrix(const FVector& CameraForward) const
{
    return BuildStableUUIDTextBillboardMatrix(CameraForward, GetWorldLocation(), GetWorldScale());
}
