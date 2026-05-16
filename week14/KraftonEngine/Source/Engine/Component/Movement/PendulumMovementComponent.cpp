#include "PendulumMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Math/MathUtils.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_CLASS_WITH_PROPERTIES(UPendulumMovementComponent, UMovementComponent)

void UPendulumMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();
	ElapsedTime = 0.0f;

	if (USceneComponent* Target = GetUpdatedComponent())
	{
		InitialRelativeRotation = Target->GetRelativeQuat();
	}
}

void UPendulumMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Target = GetUpdatedComponent();
	if (!Target)
	{
		return;
	}
	
	ElapsedTime += DeltaTime;

	// angle = Amplitude * sin(2π * Frequency * t + Phase)
	const float PhaseRad = Phase * FMath::DegToRad;
	const float AngleDeg = Amplitude * std::sin(2.0f * FMath::Pi * Frequency * ElapsedTime + PhaseRad);

	// Axis 기준 절대 회전을 쿼터니언으로 설정
	const float AngleRad = (AngleDeg + AngleOffset) * FMath::DegToRad;
	FVector NormalizedAxis = Axis.Normalized();
	if (NormalizedAxis.Length() < 0.001f)
	{
		NormalizedAxis = FVector(0.0f, 1.0f, 0.0f);
	}

	FQuat SwingQuat = FQuat::FromAxisAngle(NormalizedAxis, AngleRad);

	FQuat FinalQuat = InitialRelativeRotation * SwingQuat;

	Target->SetRelativeRotation(FinalQuat);
}

void UPendulumMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
}

void UPendulumMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
}
