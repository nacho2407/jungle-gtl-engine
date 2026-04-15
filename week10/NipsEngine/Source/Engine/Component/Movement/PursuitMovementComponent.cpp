#include "PursuitMovementComponent.h"
#include "Math/Quat.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Editor/Viewport/ViewportCamera.h"

DEFINE_CLASS(UPursuitMovementComponent, UMovementComponent)
REGISTER_FACTORY(UPursuitMovementComponent)

namespace
{
// Returns normalized direction from A to B
FVector GetNormalizedDir(const FVector& A, const FVector& B)
{
    FVector Dir = B - A;
    return Dir.GetSafeNormal();
}

float GetYaw(const FVector& NormDir)
{
    return asinf(NormDir.Z);
}

float GetPitch(const FVector& NormDir)
{
    return atan2f(NormDir.Y, NormDir.X);
}
} // namespace

void UPursuitMovementComponent::BeginPlay() {}

void UPursuitMovementComponent::TickComponent(float DeltaTime) {
	if (!IsInPursuit()) return;
	Elapsed += DeltaTime;

	if (Elapsed >= UpdateLerpInterval) {
		Elapsed = 0.f;
		UpdateTargetLoc();
	}

	UpdateLerp(DeltaTime);

	if (bFaceTargetDir)
		FaceTargetDir(DeltaTime);
}

void UPursuitMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	OutProps.push_back({ "Detection Radius",	EPropertyType::Float, &DetectionRadius , 0.01f, 4096.f, 0.01f });
    OutProps.push_back({ "Pursuit Speed",		EPropertyType::Float, &PursuitSpeed, 0.01f, 100.f, 0.01f });
    OutProps.push_back({ "Pursuit Interval",	EPropertyType::Float, &UpdateLerpInterval, 0.01f, 5.f, 0.01f });
    OutProps.push_back({ "Orient To Target",	EPropertyType::Bool,  &bFaceTargetDir });
}

void UPursuitMovementComponent::PostDuplicate(UObject* Original) {
    UActorComponent::PostDuplicate(Original);
    const UPursuitMovementComponent* Orig = Cast<UPursuitMovementComponent>(Original);

    // Copy configuration
    UpdateLerpInterval		= Orig->UpdateLerpInterval;
    DetectionRadius			= Orig->DetectionRadius;
    PursuitSpeed			= Orig->PursuitSpeed;

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
}

void UPursuitMovementComponent::UpdateLerp(float DeltaTime) {
    CurrentPoint = UpdatedComponent->GetWorldLocation();
    float Alpha  = PursuitSpeed * DeltaTime;
    Alpha        = Alpha < 1.f ? Alpha : 1.f;
    UpdatedComponent->SetWorldLocation(FVector::Lerp(CurrentPoint, TargetPoint, Alpha));
}

void UPursuitMovementComponent::FaceTargetDir(float DeltaTime)
{
    if (!UpdatedComponent)
        return;

    FVector Dir = GetNormalizedDir(CurrentPoint, TargetPoint);
    if (Dir.IsNearlyZero())
        return;

    // Build target quaternion that rotates the forward axis (+X) onto Dir.
    // Uses the half-vector trick: avoids acosf and handles all angles robustly.
    const FVector Forward = FVector::ForwardVector;
    const FVector Half = (Forward + Dir).GetSafeNormal();

    FQuat TargetQuat;
    if (Half.IsNearlyZero())
    {
        // Dir is exactly anti-parallel to Forward. Rotate 180 Deg around Up (+Z)
        TargetQuat = FQuat(FVector::UpVector, 3.14159265f);
    }
    else
    {
        FVector Axis = FVector::CrossProduct(Forward, Half);
        float W = FVector::DotProduct(Forward, Half);
        TargetQuat = FQuat(Axis.X, Axis.Y, Axis.Z, W);
        TargetQuat.Normalize();
    }

    // Slerp from the current rotation towards the target over RotateDuration
    FQuat Current = UpdatedComponent->GetRelativeQuat();
    float Alpha = 0.35f;

    UpdatedComponent->SetRelativeRotationQuat(FQuat::Slerp(Current, TargetQuat, Alpha));
}