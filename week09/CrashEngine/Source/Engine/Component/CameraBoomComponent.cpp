#include "Component/CameraBoomComponent.h"

#include <cstring>

#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_COMPONENT_CLASS(UCameraBoomComponent, USceneComponent, EEditorComponentCategory::Basic)

namespace
{
	bool IsNoneActorName(const FString& Name)
	{
	    return Name.empty() || Name == "None";
	}
	
	FString GetActorReferenceName(const AActor* Actor)
	{
	    return Actor ? Actor->GetFName().ToString() : FString("None");
	}
} // namespace

void UCameraBoomComponent::BeginPlay()
{
    USceneComponent::BeginPlay();
    ResolveTargetActor();
    UpdateCamera(0.0f, true);
}

void UCameraBoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    (void)TickType;
    (void)ThisTickFunction;

    UpdateCamera(DeltaTime, false);
}

void UCameraBoomComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << TargetActorName;
    Ar << TargetDistance;
    Ar << OffsetDirection;
    Ar << bAutoFindTargetIfUnset;
}

void UCameraBoomComponent::PostDuplicate()
{
    USceneComponent::PostDuplicate();
    TargetActor = nullptr;
}

void UCameraBoomComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Target Actor", EPropertyType::ActorRef, &TargetActorName });
    OutProps.push_back({ "Target Distance", EPropertyType::Float, &TargetDistance, 0.0f, 5000.0f, 1.0f });
    OutProps.push_back({ "Offset Direction", EPropertyType::Vec3, &OffsetDirection, 0.0f, 0.0f, 0.01f });
    OutProps.push_back({ "Auto Find Target", EPropertyType::Bool, &bAutoFindTargetIfUnset });
}

void UCameraBoomComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Target Actor") == 0)
    {
        TargetActor = nullptr;
        ResolveTargetActor();
    }
}

void UCameraBoomComponent::SetTargetActor(AActor* InTargetActor)
{
    TargetActor = InTargetActor;
    TargetActorName = GetActorReferenceName(InTargetActor);
}

AActor* UCameraBoomComponent::ResolveTargetActor()
{
    if (TargetActor)
    {
        return TargetActor;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    if (!IsNoneActorName(TargetActorName))
    {
        for (AActor* Actor : World->GetActors())
        {
            if (Actor && Actor->GetFName().ToString() == TargetActorName)
            {
                TargetActor = Actor;
                return TargetActor;
            }
        }
    }

    if (bAutoFindTargetIfUnset)
    {
        TargetActor = FindFallbackTargetActor();
        if (TargetActor && IsNoneActorName(TargetActorName))
        {
            TargetActorName = GetActorReferenceName(TargetActor);
        }
    }

    return TargetActor;
}

AActor* UCameraBoomComponent::FindFallbackTargetActor() const
{
    UWorld* World = GetWorld();
    AActor* OwnerActor = GetOwner();
    if (!World)
    {
        return nullptr;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor || Actor == OwnerActor)
        {
            continue;
        }

        bool bHasCamera = false;
        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (Cast<UCameraComponent>(Component))
            {
                bHasCamera = true;
                break;
            }
        }

        if (!bHasCamera)
        {
            return Actor;
        }
    }

    return nullptr;
}

UCameraComponent* UCameraBoomComponent::FindControlledCamera() const
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return nullptr;
    }

    UCameraComponent* FirstCamera = nullptr;
    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        UCameraComponent* Camera = Cast<UCameraComponent>(Component);
        if (!Camera)
        {
            continue;
        }

        if (!FirstCamera)
        {
            FirstCamera = Camera;
        }

        if (Camera->GetParent() == this)
        {
            return Camera;
        }
    }

    return FirstCamera;
}

bool UCameraBoomComponent::ConsumeExternalCameraLocation(const FVector& TargetLocation)
{
    const FVector CurrentLocation = GetWorldLocation();
    if (!bHasLastAppliedCameraLocation)
    {
        return false;
    }

    constexpr float ExternalMoveTolerance = 0.01f;
    if (FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) <= ExternalMoveTolerance * ExternalMoveTolerance)
    {
        return false;
    }

    FVector NewOffset = CurrentLocation - TargetLocation;
    const float NewDistance = NewOffset.Length();
    if (NewDistance <= 0.0001f)
    {
        return false;
    }

    NewOffset /= NewDistance;
    OffsetDirection = NewOffset;
    TargetDistance = NewDistance;
    LastAppliedCameraLocation = CurrentLocation;
    return true;
}

void UCameraBoomComponent::UpdateCamera(float DeltaTime, bool bSnapToTarget)
{
    AActor* Target = ResolveTargetActor();
    UCameraComponent* Camera = FindControlledCamera();
    if (!Target || !Camera)
    {
        return;
    }

    const FVector TargetLocation = Target->GetActorLocation();
    if (!bSnapToTarget)
    {
        ConsumeExternalCameraLocation(TargetLocation);
    }

    FVector Direction = OffsetDirection;
    if (Direction.LengthSquared() <= 0.0001f)
    {
        Direction = FVector(-1.0f, 0.0f, 0.0f);
    }
    Direction.Normalize();

    const FVector DesiredCameraLocation = TargetLocation + Direction * TargetDistance;

    (void)DeltaTime;
    (void)bSnapToTarget;

    SetWorldLocation(DesiredCameraLocation);
    Camera->SetWorldLocation(DesiredCameraLocation);
    Camera->LookAt(TargetLocation);
    LastAppliedCameraLocation = DesiredCameraLocation;
    bHasLastAppliedCameraLocation = true;
}
