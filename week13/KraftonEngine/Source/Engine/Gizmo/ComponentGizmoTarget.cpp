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

USceneComponent* FComponentGizmoTarget::GetComponent() const
{
    return Component.Get();
}

void FComponentGizmoTarget::SetComponent(USceneComponent* InComponent)
{
    Component = InComponent;
}

bool FComponentGizmoTarget::IsValid() const
{
    return Component.IsValid();
}

UWorld* FComponentGizmoTarget::GetWorld() const
{
    USceneComponent* Comp = Component.Get();
    return Comp ? Comp->GetWorld() : nullptr;
}

FVector FComponentGizmoTarget::GetWorldLocation() const
{
    USceneComponent* Comp = Component.Get();
    return Comp ? Comp->GetWorldLocation() : FVector::ZeroVector;
}

FRotator FComponentGizmoTarget::GetWorldRotation() const
{
    USceneComponent* Comp = Component.Get();
    return Comp ? Comp->GetWorldRotation() : FRotator::ZeroRotator;
}

FQuat FComponentGizmoTarget::GetWorldQuat() const
{
    USceneComponent* Comp = Component.Get();
    return Comp ? Comp->GetRelativeQuat() : FQuat::Identity;
}

FVector FComponentGizmoTarget::GetWorldScale() const
{
    USceneComponent* Comp = Component.Get();
    return Comp ? Comp->GetWorldScale() : FVector::OneVector;
}

void FComponentGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
    if (USceneComponent* Comp = Component.Get())
    {
        Comp->SetWorldLocation(NewLocation);
    }
}

void FComponentGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
    if (USceneComponent* Comp = Component.Get())
    {
        Comp->SetRelativeRotation(NewRotation);
    }
}

void FComponentGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
    if (USceneComponent* Comp = Component.Get())
    {
        Comp->SetRelativeRotation(NewQuat);
    }
}

void FComponentGizmoTarget::SetWorldScale(const FVector& NewScale)
{
    if (USceneComponent* Comp = Component.Get())
    {
        Comp->SetRelativeScale(NewScale);
    }
}

void FComponentGizmoTarget::AddWorldOffset(const FVector& Delta)
{
    if (USceneComponent* Comp = Component.Get())
    {
        Comp->AddWorldOffset(Delta);
    }
}

void FComponentGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
    if (USceneComponent* Comp = Component.Get())
    {
        if (!bWorldSpace)
        {
            FQuat CurrentRotation = Comp->GetRelativeQuat();
            FQuat NewRotation = CurrentRotation * Delta;
            Comp->SetRelativeRotation(NewRotation);
        }
        else
        {
            FQuat CurrentRotation = Comp->GetRelativeQuat();
            FQuat NewRotation = Delta * CurrentRotation;
            Comp->SetRelativeRotation(NewRotation);
        }
    }
}

void FComponentGizmoTarget::AddScaleDelta(const FVector& Delta)
{
    if (USceneComponent* Comp = Component.Get())
    {
        FVector NewScale = Comp->GetRelativeScale() + Delta;
        if (NewScale.X < 0.001f) NewScale.X = 0.001f;
        if (NewScale.Y < 0.001f) NewScale.Y = 0.001f;
        if (NewScale.Z < 0.001f) NewScale.Z = 0.001f;
        Comp->SetRelativeScale(NewScale);
    }
}
