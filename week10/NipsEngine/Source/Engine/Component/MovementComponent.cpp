#include "MovementComponent.h"

#include "SceneComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Spatial/WorldSpatialIndex.h"
#include "Engine/Geometry/Ray.h"
#include "Math/Utils.h"

DEFINE_CLASS(UMovementComponent, UActorComponent)

// 순수 가상 함수를 포함하는 추상 클래스이므로 REGISTER_FACTORY는 등록하지 않습니다.
// 또, 추상 클래스이므로 별도의 복사 함수를 구현하지 않고 자식 클래스에서 복사를 수행합니다.

void UMovementComponent::SetUpdatedComponent(USceneComponent* InComponent)
{
    UpdatedComponent = InComponent;
}

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);

    // UpdatedComponent 는 SceneComponentRef 타입으로 노출됩니다.
    // CopyPropertiesFrom 은 포인터 복원을 건너뛰며, Actor::Duplicate() 에서 재매핑합니다.
    OutProps.push_back({ "Updated Component",        EPropertyType::SceneComponentRef, &UpdatedComponent });
    OutProps.push_back({ "Velocity",                 EPropertyType::Vec3, &Velocity });
    OutProps.push_back({ "Plane Constraint Normal",  EPropertyType::Vec3, &PlaneConstraintNormal });
    OutProps.push_back({ "Update Only If Rendered",  EPropertyType::Bool, &bUpdateOnlyIfRendered });
    OutProps.push_back({ "Constrain To Plane",       EPropertyType::Bool, &bConstrainToPlane });
}

// UpdatedComponent를 Delta만큼 이동시킵니다.
void UMovementComponent::MoveUpdatedComponent(const FVector& Delta)
{
    if (UpdatedComponent == nullptr)
    {
        return;
    }

    FVector ConstrainedDelta = bConstrainToPlane ? ConstrainDirectionToPlane(Delta) : Delta;

    if (ConstrainedDelta.IsNearlyZero())
    {
        return;
    }

    UpdatedComponent->AddWorldOffset(ConstrainedDelta);
}

void UMovementComponent::AddInputVector(const FVector& WorldDirection, float Scale)
{
    PendingInputVector += WorldDirection * Scale;
}

FVector UMovementComponent::ConsumeInputVector()
{
    const FVector Consumed = PendingInputVector;
    PendingInputVector     = FVector::ZeroVector;
    return Consumed;
}

bool UMovementComponent::IsExceedingMaxSpeed(float MaxSpeed) const
{
    if (MaxSpeed < 0.0f)
    {
        return false;
    }

    return Velocity.SizeSquared() > MaxSpeed * MaxSpeed;
}

// 평면에 투영된 법선벡터를 반환한다.
FVector UMovementComponent::ConstrainDirectionToPlane(const FVector& Direction) const
{
    if (!bConstrainToPlane)
    {
        return Direction;
    }

	// 방향벡터를 평면의 정규화된 법선벡터에 내적한다.
    const FVector Normal = PlaneConstraintNormal.GetSafeNormal();
    const float Dot = FVector::DotProduct(Direction, Normal);
    return Direction - Normal * Dot;
}

// 원점 기준 평면에 투영된 점의 위치벡터를 반환한다.
FVector UMovementComponent::ConstrainLocationToPlane(const FVector& Location) const
{
    if (!bConstrainToPlane)
    {
        return Location;
    }
 
    const FVector Normal = PlaneConstraintNormal.GetSafeNormal();
    const float Dot = FVector::DotProduct(Location, Normal);
    return Location - Normal * Dot;
}
