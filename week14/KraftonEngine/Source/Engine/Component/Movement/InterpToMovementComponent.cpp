#include "InterpToMovementComponent.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Quat.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UInterpToMovementComponent, UMovementComponent)

namespace {
	// Returns normalized direction from A to B
	FVector GetNormalizedDir(const FVector& A, const FVector& B) {
		FVector Dir = B - A;
		return Dir.Normalized();
	}

	float GetYaw(const FVector& NormDir) {
		return asinf(NormDir.Z);
	}

	float GetPitch(const FVector& NormDir) {
		return atan2f(NormDir.Y, NormDir.X);
	}
}

// --- Overrides ---------------------------------------------------------
void UInterpToMovementComponent::BeginPlay() {
	UMovementComponent::BeginPlay();

	if (!UpdatedComponent) return;

	for (auto& ControlPoint : ControlPoints) {
		ControlPoint += UpdatedComponent->GetWorldLocation();
	}
	if (bAutoActivate)
	{
		Initiate();
	}
}

void UInterpToMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) {
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bisLerping) return;
	Elapsed += DeltaTime;

	// FaceTargetDir must run before UpdateLerp — UpdateLerp can advance PointIDs via DestinationReached
	FaceTargetDir(DeltaTime);
	UpdateLerp(DeltaTime);
}

void UInterpToMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UMovementComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Auto Activate",		  EPropertyType::Bool,		 &bAutoActivate });
	OutProps.push_back({ "Orient To Movement",	  EPropertyType::Bool,		 &bFaceTargetDir });
	OutProps.push_back({ "Interp Duration",		  EPropertyType::Float,      &Duration,       0.1f, 2048.0f, 0.1f });
	static const char* InterpBehaviourNames[] = { "One Shot", "One Shot Reverse", "Loop", "Ping-Pong" };
	OutProps.push_back({ "Interp Mode",			  EPropertyType::Enum,		 &InterpBehaviour, 0,0,0, InterpBehaviourNames, 4 });
	OutProps.push_back({ "Control Points",		  EPropertyType::Vec3Array,  &ControlPoints });
}

void UInterpToMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);
	Ar << InterpBehaviour;
	Ar << Duration;
	Ar << bAutoActivate;
	Ar << bFaceTargetDir;
	Ar << ControlPoints;
}

// --- Control Point Management--------------------------------------------
void UInterpToMovementComponent::AddControlPoint(FVector InControlPoint) {
	ControlPoints.push_back(InControlPoint);
	Reset();
}

void UInterpToMovementComponent::RemoveControlPoint(uint32 Index) {
	if (Index >= ControlPoints.size()) return;
	ControlPoints.erase(ControlPoints.begin() + Index);
}

FVector& UInterpToMovementComponent::GetControlPoint(uint32 Index) {
	assert(Index < ControlPoints.size());
	return ControlPoints[Index];
}

void UInterpToMovementComponent::SetControlPoint(uint32 Index, FVector InPoint) {
	if (Index >= ControlPoints.size()) return;
	ControlPoints[Index] = InPoint;
}

// --- Interpolation Duration ---------------------------------------------
void UInterpToMovementComponent::SetInterpDuration(float InDuration) {
	if (InDuration <= 0) return;
	Duration = InDuration;
}

// --- Interpolation Behaviour --------------------------------------------
void UInterpToMovementComponent::SetInterpolationBehaviour(EInterpBehaviour InBehaviour) {
	Reset();
	InterpBehaviour = InBehaviour;
}

// --- Misc ---------------------------------------------------------------
void UInterpToMovementComponent::Initiate() {
	if (ControlPoints.size() <= 1) return;
	Reset();
	bisLerping = true;

	for (size_t i = ControlPoints.size() - 1; i > 0; i--)
	{
		TotalDistance += FVector::Distance(ControlPoints[i], ControlPoints[i - 1]);
	}

	SetNextDistRatio();

	// Snap to target direction upon init if flagged
	if (bFaceTargetDir && ControlPoints.size() >= 2) {
		FVector NormDir = GetNormalizedDir(ControlPoints[0], ControlPoints[1]);
		TargetYaw = GetYaw(NormDir);
		TargetPitch = GetPitch(NormDir);
		RotateDuration = 0.01f;
	}
}

void UInterpToMovementComponent::Reset() {
	bPing = true;
	Elapsed = 0.f;
	CurrentPointID = 0;
	NextPointID = 1;
	TotalDistance = 0;

	if (!ControlPoints.empty() && UpdatedComponent) {
		UpdatedComponent->SetWorldLocation(ControlPoints.front());
	}
}

void UInterpToMovementComponent::ResetAndHalt() {
	Reset();
	bisLerping = false;
}

// --- Private -----------------------------------------------------------
void UInterpToMovementComponent::Ping() {
	// Forward
	if (bPing) return;
	bPing = true;
	CurrentPointID = 0;
	NextPointID = 1;
}

void UInterpToMovementComponent::Pong() {
	// Backward
	if (!bPing) return;
	bPing = false;
	CurrentPointID = (uint32)(ControlPoints.size() - 1);
	NextPointID = (uint32)(ControlPoints.size() - 2);
}

void UInterpToMovementComponent::UpdateLerp(float DeltaTime) {
	if (!UpdatedComponent) return;
	if (ControlPoints.size() <= 1 || NextPointID >= ControlPoints.size())
		return;
	float Alpha = (Elapsed / (Duration * NextDistRatio));
	UpdatedComponent->SetWorldLocation(FVector::Lerp(ControlPoints[CurrentPointID], ControlPoints[NextPointID], Alpha));

	// Check if target point has been reached
	if (Alpha >= 1.f)
	{
		DestinationReached();
	}
}

void UInterpToMovementComponent::FaceTargetDir(float DeltaTime) {
	if (!UpdatedComponent) return;
	if (!bFaceTargetDir || NextPointID >= ControlPoints.size()) return;

	FVector Dir = GetNormalizedDir(ControlPoints[CurrentPointID], ControlPoints[NextPointID]);
	if (Dir.IsNearlyZero()) return;

	// Build target quaternion that rotates the forward axis (+X) onto Dir.
	// Uses the half-vector trick: avoids acosf and handles all angles robustly.
	const FVector Forward = FVector::ForwardVector;
	const FVector Half = (Forward + Dir).Normalized();

	FQuat TargetQuat;
	if (Half.IsNearlyZero())
	{
		// Dir is exactly anti-parallel to Forward. Rotate 180 Deg around Up (+Z)
		TargetQuat = FQuat::FromAxisAngle(FVector::UpVector, 3.14159265f);
	}
	else
	{
		FVector Axis = Forward.Cross(Half);
		float   W    = Forward.Dot(Half);
		TargetQuat   = FQuat(Axis.X, Axis.Y, Axis.Z, W);
		TargetQuat.Normalize();
	}

	// Slerp from the current rotation towards the target over RotateDuration
	FQuat Current = UpdatedComponent->GetRelativeQuat();
	float Alpha = (RotateDuration > 0.f) ? (DeltaTime / RotateDuration) : 1.f;
	Alpha = Alpha < 1.f ? Alpha : 1.f;

	UpdatedComponent->SetRelativeRotation(FQuat::Slerp(Current, TargetQuat, Alpha));
}

void UInterpToMovementComponent::DestinationReached() {
	if (bPing && NextPointID >= ControlPoints.size() - 1) {
		EndOfChain();
		return;
	}

	if (!bPing && NextPointID <= 0)
	{
		EndOfChain();
		return;
	}

	Elapsed = 0;
	if (InterpBehaviour == EInterpBehaviour::PingPong && !bPing) {
		CurrentPointID -= 1;
		NextPointID -= 1;
	}
	else {
		CurrentPointID += 1;
		NextPointID += 1;
	}
	SetNextDistRatio();
	SetRotationSpeed();
}

void UInterpToMovementComponent::EndOfChain() {
	switch (InterpBehaviour)
	{
	case EInterpBehaviour::OneShot:
	{
		bisLerping = false;
		return;
	}
	case EInterpBehaviour::OneShotReverse:
	{
		ResetAndHalt();
		return;
	}
	case EInterpBehaviour::Loop:
	{
		float CachedDist = TotalDistance;
		Reset();
		TotalDistance = CachedDist;
		SetNextDistRatio();
		return;
	}
	case EInterpBehaviour::PingPong:
	{
		Elapsed = 0;
		if (bPing)
			Pong();
		else
			Ping();
		return;
	}
	}
}

void UInterpToMovementComponent::SetNextDistRatio() {
	if (TotalDistance == 0) {
		NextDistRatio = 0;
		return;
	}

	NextDistRatio =
		FVector::Distance(ControlPoints[CurrentPointID], ControlPoints[NextPointID]) / TotalDistance;
}

void UInterpToMovementComponent::SetRotationSpeed() {
	float InvSpeed = Duration / (TotalDistance * NextDistRatio);
	float MinDuration = 0.8f;
	RotateDuration = MinDuration < InvSpeed ? MinDuration : InvSpeed;
}