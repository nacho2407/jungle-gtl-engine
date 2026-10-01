#include "CapsuleComponent.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_COMPONENT_CLASS(UCapsuleComponent, UShapeComponent, EEditorComponentCategory::Shapes)

UCapsuleComponent::UCapsuleComponent()
    : CapsuleHalfHeight(1.0f),
      CapsuleRadius(1.0f),
      CapsuleCollision(FVector(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, FVector(0.0f, 0.0f, 1.0f))
{
}

void UCapsuleComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << CapsuleHalfHeight;
    Ar << CapsuleRadius;
    Ar << CapsuleCollision;
}

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "CapsuleHalfHeight", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 100.f, 0.01f });
    OutProps.push_back({ "CapsuleRadius", EPropertyType::Float, &CapsuleRadius, 0.0f, 100.0f, 0.01f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
    SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
    UShapeComponent::PostEditProperty(PropertyName);
}

void UCapsuleComponent::SetCapsuleSize(float Radius, float HalfHeight)
{
    CapsuleRadius = std::max(0.0f, Radius);
    CapsuleHalfHeight = std::max(0.0f, HalfHeight);
    OnTransformDirty();
}

void UCapsuleComponent::SetCapsuleRadius(float Radius)
{
    SetCapsuleSize(Radius, CapsuleHalfHeight);
}

void UCapsuleComponent::SetCapsuleHalfHeight(float HalfHeight)
{
    SetCapsuleSize(CapsuleRadius, HalfHeight);
}

void UCapsuleComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

// CapsuleComponent.cpp
void UCapsuleComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    // 1. 순수 월드 스케일 구하기
    FVector PureWorldScale = GetRelativeTransform().Scale;
    USceneComponent* CurrentParent = GetParent();
    while (CurrentParent != nullptr)
    {
        FVector ParentScale = CurrentParent->GetRelativeTransform().Scale;
        PureWorldScale.X *= ParentScale.X;
        PureWorldScale.Y *= ParentScale.Y;
        PureWorldScale.Z *= ParentScale.Z;
        CurrentParent = CurrentParent->GetParent();
    }

    // 2. 순수 월드 회전 쿼터니언 구하기
    FQuat WorldQuat =GetRelativeTransform().Rotation;
    USceneComponent* CurrentRotParent = GetParent();
    while (CurrentRotParent != nullptr)
    {
        FQuat ParentQuat = CurrentRotParent->GetRelativeTransform().Rotation;
        WorldQuat = ParentQuat * WorldQuat;
        CurrentRotParent = CurrentRotParent->GetParent();
    }
    WorldQuat.Normalize();

    // 3. 캡슐 데이터 업데이트
    CapsuleCollision.Center = GetWorldLocation();

    const float MaxScaleXY = std::max(std::abs(PureWorldScale.X), std::abs(PureWorldScale.Y));
    CapsuleCollision.Radius = CapsuleRadius * MaxScaleXY;
    CapsuleCollision.HalfHeight = CapsuleHalfHeight * std::abs(PureWorldScale.Z);

    // [핵심] 행렬에서 값을 뽑을 필요 없이, 쿼터니언 자체에서 바로 Up 방향을 가져옵니다.
    CapsuleCollision.UpVector = WorldQuat.GetUpVector().Normalized();

    NotifyCollisionShapeChanged();
}

void UCapsuleComponent::UpdateWorldAABB() const
{
    const FVector Up = CapsuleCollision.UpVector.Normalized();
    const float Radius = CapsuleCollision.Radius;
    const float HalfHeight = CapsuleCollision.HalfHeight;

    const FVector Extent(
        std::abs(Up.X) * HalfHeight + Radius,
        std::abs(Up.Y) * HalfHeight + Radius,
        std::abs(Up.Z) * HalfHeight + Radius);

    WorldAABBMinLocation = CapsuleCollision.Center - Extent;
    WorldAABBMaxLocation = CapsuleCollision.Center + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}
