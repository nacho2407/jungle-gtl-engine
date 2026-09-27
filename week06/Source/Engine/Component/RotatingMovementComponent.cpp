#include "RotatingMovementComponent.h"

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
    if (!UpdatedSceneComponent) return;

    const FRotator DeltaRota = RotationRate * DeltaTime;

    // 쿼터니언으로 회전 누적 → 짐벌락 방지
    const FQuat OldQuat = UpdatedSceneComponent->GetRelativeRotation().ToQuaternion();
    const FQuat DeltaQuat = DeltaRota.ToQuaternion();
    const FQuat NewQuat = OldQuat * DeltaQuat;

    // Pivot 보정
    const FMatrix OldMat = OldQuat.ToMatrix();
    const FMatrix NewMat = NewQuat.ToMatrix();
    const FVector OldPivot = PivotTranslation * OldMat;
    const FVector NewPivot = PivotTranslation * NewMat;

    UpdatedSceneComponent->SetRelativeLocation(
        UpdatedSceneComponent->GetRelativeLocation() + (OldPivot - NewPivot)
    );
    UpdatedSceneComponent->AddLocalRotation(DeltaQuat);
}

void URotatingMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << RotationRate.Pitch;
	Ar << RotationRate.Yaw;
	Ar << RotationRate.Roll;
	Ar << PivotTranslation;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "PivotTransform", EPropertyType::Rotator, &PivotTranslation, 0.0f, 0.0f, 0.0f });
}
