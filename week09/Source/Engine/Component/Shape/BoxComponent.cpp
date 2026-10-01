#include "BoxComponent.h"
#include "Serialization/Archive.h"

#include "GameFrameWork/World.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_COMPONENT_CLASS(UBoxComponent, UShapeComponent, EEditorComponentCategory::Shapes)

UBoxComponent::UBoxComponent()
    : BoxExtent(1.0f, 1.0f, 1.0f),
      BoxCollision(FOBB(FVector(0.0f, 0.0f, 0.0f), FVector(0.5f, 0.5f, 0.5f), FRotator(0.0f, 0.0f, 0.0f)))
{
}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxCollision;
    Ar << BoxExtent;
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BoxExtent", EPropertyType::Vec3, &BoxExtent, 0.f, 100.0f, 0.1f });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
    SetBoxExtent(BoxExtent);
    UShapeComponent::PostEditProperty(PropertyName);
}

void UBoxComponent::SetBoxExtent(const FVector& Extent)
{
    BoxExtent.X = std::max(0.0f, Extent.X);
    BoxExtent.Y = std::max(0.0f, Extent.Y);
    BoxExtent.Z = std::max(0.0f, Extent.Z);
    OnTransformDirty();
}

void UBoxComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

// BoxComponent.cpp
void UBoxComponent::OnTransformDirty()
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

    // ------------------------------------------------------------------
    // 2. 순수 월드 회전 행렬 구하기 (쿼터니언 곱셈 버리고 행렬로 직접 누적!)
    // ------------------------------------------------------------------
    FMatrix RotMat = GetRelativeTransform().Rotation.ToMatrix();
    USceneComponent* CurrentRotParent = GetParent();

    while (CurrentRotParent != nullptr)
    {
        FMatrix ParentRotMat = CurrentRotParent->GetRelativeTransform().Rotation.ToMatrix();

        // SceneComponent.cpp의 UpdateWorldMatrix()와 완벽하게 동일한 곱셈 순서 (자식 * 부모)
        RotMat = RotMat * ParentRotMat;

        CurrentRotParent = CurrentRotParent->GetParent();
    }

    // 3. 조립 (S * R * T)
    FVector PureWorldLocation = GetWorldLocation();
    FVector FinalBoxScale = FVector(BoxExtent.X *abs( PureWorldScale.X), BoxExtent.Y *abs(  PureWorldScale.Y), BoxExtent.Z * abs( PureWorldScale.Z));

    FMatrix ScaleMat = FMatrix::MakeScaleMatrix(FinalBoxScale);
    FMatrix TransMat = FMatrix::MakeTranslationMatrix(PureWorldLocation);

    FMatrix CollisionWorldMat = ScaleMat * RotMat * TransMat;

    BoxCollision.Bounds.UpdateAsOBB(CollisionWorldMat);
    NotifyCollisionShapeChanged();
}
void UBoxComponent::UpdateWorldAABB() const
{
    const FOBB& OBB = BoxCollision.Bounds;

    const FVector AxisX = OBB.Rotation.GetForwardVector();
    const FVector AxisY = OBB.Rotation.GetRightVector();
    const FVector AxisZ = OBB.Rotation.GetUpVector();

    const FVector Extent(
        std::abs(AxisX.X) * OBB.Extent.X + std::abs(AxisY.X) * OBB.Extent.Y + std::abs(AxisZ.X) * OBB.Extent.Z,
        std::abs(AxisX.Y) * OBB.Extent.X + std::abs(AxisY.Y) * OBB.Extent.Y + std::abs(AxisZ.Y) * OBB.Extent.Z,
        std::abs(AxisX.Z) * OBB.Extent.X + std::abs(AxisY.Z) * OBB.Extent.Y + std::abs(AxisZ.Z) * OBB.Extent.Z);

    WorldAABBMinLocation = OBB.Center - Extent;
    WorldAABBMaxLocation = OBB.Center + Extent;
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}
