#include "Gizmo/PhysicsAssetConstraintGizmoTarget.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "GameFramework/World.h"
#include "Physics/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

FPhysicsAssetConstraintGizmoTarget::FPhysicsAssetConstraintGizmoTarget(
	UPhysicsAssetDebugComponent* InDebugComponent,
	int32 InConstraintIndex)
	: DebugComponent(InDebugComponent)
	, ConstraintIndex(InConstraintIndex)
{
}

void FPhysicsAssetConstraintGizmoTarget::SetConstraint(
	UPhysicsAssetDebugComponent* InDebugComponent,
	int32 InConstraintIndex)
{
	DebugComponent = InDebugComponent;
	ConstraintIndex = InConstraintIndex;
}

void FPhysicsAssetConstraintGizmoTarget::Clear()
{
	DebugComponent = nullptr;
	ConstraintIndex = -1;
}

bool FPhysicsAssetConstraintGizmoTarget::IsValid() const
{
	FVector WorldLocation = FVector::ZeroVector;
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	FConstraintInstanceInitDesc* ConstraintDesc = GetConstraintDesc();
	return Component && ConstraintDesc && Component->GetConstraintWorldLocation(*ConstraintDesc, WorldLocation);
}

UWorld* FPhysicsAssetConstraintGizmoTarget::GetWorld() const
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	return Component ? Component->GetWorld() : nullptr;
}

FVector FPhysicsAssetConstraintGizmoTarget::GetWorldLocation() const
{
	FVector WorldLocation = FVector::ZeroVector;
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	FConstraintInstanceInitDesc* ConstraintDesc = GetConstraintDesc();
	if (!Component || !ConstraintDesc)
	{
		return WorldLocation;
	}

	Component->GetConstraintWorldLocation(*ConstraintDesc, WorldLocation);
	return WorldLocation;
}

FRotator FPhysicsAssetConstraintGizmoTarget::GetWorldRotation() const
{
	return FRotator::ZeroRotator;
}

FQuat FPhysicsAssetConstraintGizmoTarget::GetWorldQuat() const
{
	return FQuat::Identity;
}

FVector FPhysicsAssetConstraintGizmoTarget::GetWorldScale() const
{
	return FVector::OneVector;
}

void FPhysicsAssetConstraintGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	FConstraintInstanceInitDesc* ConstraintDesc = GetConstraintDesc();
	if (!Component || !ConstraintDesc)
	{
		return;
	}

	Component->SetConstraintWorldLocation(*ConstraintDesc, NewLocation);
}

void FPhysicsAssetConstraintGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	(void)NewRotation;
}

void FPhysicsAssetConstraintGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
	(void)NewQuat;
}

void FPhysicsAssetConstraintGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	(void)NewScale;
}

void FPhysicsAssetConstraintGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	SetWorldLocation(GetWorldLocation() + Delta);
}

void FPhysicsAssetConstraintGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	(void)Delta;
	(void)bWorldSpace;
}

void FPhysicsAssetConstraintGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	(void)Delta;
}

UPhysicsAssetDebugComponent* FPhysicsAssetConstraintGizmoTarget::GetDebugComponent() const
{
	return DebugComponent.Get();
}

FConstraintInstanceInitDesc* FPhysicsAssetConstraintGizmoTarget::GetConstraintDesc() const
{
	UPhysicsAssetDebugComponent* Component = GetDebugComponent();
	UPhysicsAsset* PhysicsAsset = Component ? Component->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset || ConstraintIndex < 0)
	{
		return nullptr;
	}

	TArray<FConstraintInstanceInitDesc>& ConstraintDescs = PhysicsAsset->GetConstraintInitDescsMutable();
	if (ConstraintIndex >= static_cast<int32>(ConstraintDescs.size()))
	{
		return nullptr;
	}

	return &ConstraintDescs[ConstraintIndex];
}
