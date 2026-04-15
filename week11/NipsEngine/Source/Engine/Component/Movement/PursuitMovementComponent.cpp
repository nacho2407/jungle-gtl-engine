#include "PursuitMovementComponent.h"
#include "Math/Quat.h"
#include "Object/ObjectFactory.h"
#include "Editor/Viewport/ViewportCamera.h"

DEFINE_CLASS(UPursuitMovementComponent, UMovementComponent)
REGISTER_FACTORY(UPursuitMovementComponent)

void UPursuitMovementComponent::BeginPlay() {}

void UPursuitMovementComponent::TickComponent(float DeltaTime) {
	if (!IsInPursuit()) return;
	Elapsed += DeltaTime;

	if (DeltaTime >= UpdateLerpInterval) {
		Elapsed = 0.f;
		UpdateTargetLoc();
	}

	UpdateLerp(DeltaTime);
}

void UPursuitMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	OutProps.push_back({ "Detection Radius",	EPropertyType::Float, &DetectionRadius , 0.01f, 4096.f, 0.01f });
    OutProps.push_back({ "Pursuit Speed",		EPropertyType::Float, &PursuitSpeedMultiplier, 0.01f, 100.f, 0.01f });
    OutProps.push_back({ "Pursuit Interval",	EPropertyType::Float, &UpdateLerpInterval, 0.01f, 5.f, 0.01f });
    OutProps.push_back({ "Orient To Target",	EPropertyType::Bool,  &bFaceTargetDir });
}

void UPursuitMovementComponent::PostDuplicate(UObject* Original) {
    UActorComponent::PostDuplicate(Original);
    const UPursuitMovementComponent* Orig = Cast<UPursuitMovementComponent>(Original);

    // Copy configuration
    UpdateLerpInterval		= Orig->UpdateLerpInterval;
    DetectionRadius			= Orig->DetectionRadius;
    PursuitSpeedMultiplier	= Orig->PursuitSpeedMultiplier;

    Elapsed = 0.f;
}

void UPursuitMovementComponent::SetPursuitTarget(FViewportCamera* InTarget) {
	if (InTarget) Target = InTarget;
}

void UPursuitMovementComponent::ClearTarget() {
	Target = nullptr;
}

bool UPursuitMovementComponent::IsInPursuit() const {
	if (!Target || !bIsActive) return false;
	return true;
}

void UPursuitMovementComponent::UpdateTargetLoc() {
	if (!IsInPursuit()) return;
	TargetPoint = Target->GetLocation();

	// Set Target Yaw / Pitch
}

void UPursuitMovementComponent::FaceTargetDir(float DeltaTime) {
	
}