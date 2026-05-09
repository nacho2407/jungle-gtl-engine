#include "ComponentGizmoTarget.h"
#include "Component/SceneComponent.h"

FComponentGizmoTarget::FComponentGizmoTarget()
	: Component(nullptr)
{
}

FComponentGizmoTarget::FComponentGizmoTarget(USceneComponent* InComponent)
	: Component(InComponent)
{
}

bool FComponentGizmoTarget::IsValid() const
{
	return Component != nullptr;
}

UWorld* FComponentGizmoTarget::GetWorld() const
{
	return Component ? Component->GetWorld() : nullptr;
}

FVector FComponentGizmoTarget::GetWorldLocation() const
{
	return Component ? Component->GetWorldLocation() : FVector::ZeroVector;
}

FRotator FComponentGizmoTarget::GetWorldRotation() const
{
	return Component ? Component->GetWorldRotation() : FRotator::ZeroRotator;
}

FVector FComponentGizmoTarget::GetWorldScale() const
{
	return Component ? Component->GetWorldScale() : FVector::OneVector;
}

void FComponentGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	if (Component)
	{
		Component->SetWorldLocation(NewLocation);
	}
}

void FComponentGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	if (Component)
	{
		Component->SetRelativeRotation(NewRotation);
	}
}

void FComponentGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	if (Component)
	{
		Component->SetRelativeScale(NewScale);
	}
}

void FComponentGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	if (Component)
	{
		Component->AddWorldOffset(Delta);
	}
}

void FComponentGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	if (Component)
	{
		Component->AddLocalRotation(Delta);
	}
}

void FComponentGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	if (Component)
	{
		FVector NewScale = Component->GetRelativeScale() + Delta;
		if (NewScale.X < 0.001f) NewScale.X = 0.001f;
		if (NewScale.Y < 0.001f) NewScale.Y = 0.001f;
		if (NewScale.Z < 0.001f) NewScale.Z = 0.001f;
		Component->SetRelativeScale(NewScale);
	}
}
