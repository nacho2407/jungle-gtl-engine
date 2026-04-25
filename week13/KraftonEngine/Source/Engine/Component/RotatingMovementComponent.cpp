#include "RotatingMovementComponent.h"

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Serialization/Archive.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* UpdatedSceneComponent = GetUpdatedComponent();
	if (!UpdatedSceneComponent)
	{
		return;
	}

	const FRotator DeltaRotation = RotationRate * DeltaTime;
	if (DeltaRotation.IsNearlyZero())
	{
		return;
	}

	const FQuat DeltaQuat = DeltaRotation.ToQuaternion();
	const FVector OldWorldLocation = UpdatedSceneComponent->GetWorldLocation();
	const FQuat OldWorldQuat = UpdatedSceneComponent->GetWorldMatrix().ToQuat().GetNormalized();
	const bool bHasPivotTranslation = PivotTranslation.Length() > 0.0f;

	if (bRotationInLocalSpace)
	{
		bWorldPivotInitialized = false;
		CachedWorldPivotComponent = nullptr;

		const FVector OldPivotOffsetWorld = bHasPivotTranslation
			? OldWorldQuat.RotateVector(PivotTranslation)
			: FVector(0.0f, 0.0f, 0.0f);

		// AddLocalRotation은 내부에서 quat 합성으로 누적하므로 짐벌락에 안전.
		UpdatedSceneComponent->AddLocalRotation(DeltaQuat);
		const FQuat NewWorldQuat = UpdatedSceneComponent->GetWorldMatrix().ToQuat().GetNormalized();
		if (bHasPivotTranslation)
		{
			const FVector NewPivotOffsetWorld = NewWorldQuat.RotateVector(PivotTranslation);
			const FVector PivotWorldLocation = OldWorldLocation - OldPivotOffsetWorld;
			UpdatedSceneComponent->SetWorldLocation(PivotWorldLocation + NewPivotOffsetWorld);
		}
	}
	else
	{
		const bool bPivotTranslationChanged =
			FVector::DistSquared(CachedWorldPivotTranslation, PivotTranslation) > 1e-6f;
		if (!bWorldPivotInitialized || CachedWorldPivotComponent != UpdatedSceneComponent || bPivotTranslationChanged)
		{
			CachedWorldPivotLocation = OldWorldLocation - OldWorldQuat.RotateVector(PivotTranslation);
			CachedWorldPivotTranslation = PivotTranslation;
			CachedWorldPivotComponent = UpdatedSceneComponent;
			bWorldPivotInitialized = true;
		}

		if (!bHasPivotTranslation)
		{
			return;
		}

		const FVector OldOrbitOffsetWorld = OldWorldLocation - CachedWorldPivotLocation;
		const FVector NewOrbitOffsetWorld = DeltaQuat.RotateVector(OldOrbitOffsetWorld);
		UpdatedSceneComponent->SetWorldLocation(CachedWorldPivotLocation + NewOrbitOffsetWorld);
	}
}

void URotatingMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << RotationRate.Pitch;
	Ar << RotationRate.Yaw;
	Ar << RotationRate.Roll;
	Ar << bRotationInLocalSpace;
	Ar << PivotTranslation;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Rotator, &RotationRate, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Rotation In Local Space", EPropertyType::Bool, &bRotationInLocalSpace, 0.0f, 0.0f, 0.0f });
	OutProps.push_back({ "Pivot Translation", EPropertyType::Vec3, &PivotTranslation, 0.0f, 0.0f, 0.1f });
}
