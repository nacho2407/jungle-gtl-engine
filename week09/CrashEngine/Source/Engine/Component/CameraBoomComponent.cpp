#include "Component/CameraBoomComponent.h"

#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldContext.h"
#include "Input/InputSystem.h"
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

    FVector GetSafeOffsetDirection(const FVector& Direction)
    {
        FVector Result = Direction;
        if (Result.LengthSquared() <= 0.0001f)
        {
            Result = FVector(-1.0f, 0.0f, 0.0f);
        }
        Result.Normalize();
        return Result;
    }

    FVector GetPlanarDirection(const FVector& Direction)
    {
        FVector Planar(Direction.X, Direction.Y, 0.0f);
        if (Planar.LengthSquared() <= 0.0001f)
        {
            Planar = FVector(-1.0f, 0.0f, 0.0f);
        }
        Planar.Normalize();
        return Planar;
    }

    FVector BuildOffsetDirectionFromYawPitch(float YawRad, float PitchRad)
    {
        const float CosPitch = cosf(PitchRad);
        return FVector(cosf(YawRad) * CosPitch, sinf(YawRad) * CosPitch, sinf(PitchRad));
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

//Get Actor That has Name Same as selected from Editor
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

    return TargetActor;
}

//Get Camera Attached to parent CameraActor
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

void UCameraBoomComponent::ApplyMouseOrbitInput()
{
    if (!bEnableMouseOrbit)
    {
        return;
    }

    //Editor World가 아닐때만
    UWorld* World = GetWorld();
    if (!World || World->GetWorldType() == EWorldType::Editor)
    {
        return;
    }

    const FInputSnapshot& Input = InputSystem::Get().GetSnapshot();
    if (bRequireRightMouseForOrbit && !Input.KeyDown[VK_RBUTTON])
    {
        return;
    }

    const float MouseX = static_cast<float>(Input.MouseDelta.x);
    const float MouseY = static_cast<float>(Input.MouseDelta.y);
    if (MouseX == 0.0f && MouseY == 0.0f)
    {
        return;
    }

    FVector Direction = GetSafeOffsetDirection(OffsetDirection);
    const FVector Planar = GetPlanarDirection(Direction);

    float Yaw = atan2f(Planar.Y, Planar.X);
    float Pitch = asinf(Clamp(Direction.Z, -1.0f, 1.0f));

    Yaw += MouseX * MouseOrbitSensitivity * DEG_TO_RAD;
    Pitch += -MouseY * MouseOrbitSensitivity * DEG_TO_RAD;

    const float MinPitch = FMath::Clamp(MinPitchDegrees, -89.0f, 89.0f) * DEG_TO_RAD;
    const float MaxPitch = FMath::Clamp(MaxPitchDegrees, -89.0f, 89.0f) * DEG_TO_RAD;
    const float LowPitch = MinPitch <= MaxPitch ? MinPitch : MaxPitch;
    const float HighPitch = MinPitch <= MaxPitch ? MaxPitch : MinPitch;
    Pitch = Clamp(Pitch, LowPitch, HighPitch);

    OffsetDirection = BuildOffsetDirectionFromYawPitch(Yaw, Pitch);
}

bool UCameraBoomComponent::ConsumeExternalCameraLocation(const FVector& TargetLocation)
{
    const FVector CurrentLocation = GetWorldLocation();
    if (!bHasLastAppliedCameraLocation)
    {
        return false;
    }

    //에디터, 기즈모를 제외하고 스스로 움직였을때의 값과 차이가 없으면 그대로 둔다.
    constexpr float ExternalMoveTolerance = 0.01f;
    if (FVector::DistSquared(CurrentLocation, LastAppliedCameraLocationByBoom) <= ExternalMoveTolerance * ExternalMoveTolerance)
    {
        return false;
    }

    //에디터,기즈모로 이동했다면
    FVector NewOffset = CurrentLocation - TargetLocation;
    const float NewDistance = NewOffset.Length();
    if (NewDistance <= 0.0001f)
    {
        return false;
    }

    NewOffset /= NewDistance;
    OffsetDirection = NewOffset;
    TargetDistance = NewDistance;
    LastAppliedCameraLocationByBoom = CurrentLocation;
    return true;
}

void UCameraBoomComponent::UpdateCamera(float DeltaTime, bool bSnapToTarget)
{
    (void)DeltaTime;

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

    ApplyMouseOrbitInput();

    const FVector Direction = GetSafeOffsetDirection(OffsetDirection);
    const FVector DesiredCameraLocation = TargetLocation + Direction * TargetDistance;

    SetWorldLocation(DesiredCameraLocation);
    Camera->SetWorldLocation(DesiredCameraLocation);
    Camera->LookAt(TargetLocation);

    LastAppliedCameraLocationByBoom = DesiredCameraLocation;
    bHasLastAppliedCameraLocation = true;
}
//------ Seperate with Logic ------
void UCameraBoomComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << TargetActorName;
    Ar << TargetDistance;
    Ar << OffsetDirection;
    Ar << bEnableMouseOrbit;
    Ar << bRequireRightMouseForOrbit;
    Ar << MouseOrbitSensitivity;
    Ar << MinPitchDegrees;
    Ar << MaxPitchDegrees;
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
    OutProps.push_back({ "Enable Mouse Orbit", EPropertyType::Bool, &bEnableMouseOrbit });
    OutProps.push_back({ "Require Right Mouse", EPropertyType::Bool, &bRequireRightMouseForOrbit });
    OutProps.push_back({ "Mouse Orbit Sensitivity", EPropertyType::Float, &MouseOrbitSensitivity, 0.0f, 2.0f, 0.01f });
    OutProps.push_back({ "Min Pitch", EPropertyType::Float, &MinPitchDegrees, -89.0f, 89.0f, 1.0f });
    OutProps.push_back({ "Max Pitch", EPropertyType::Float, &MaxPitchDegrees, -89.0f, 89.0f, 1.0f });
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
