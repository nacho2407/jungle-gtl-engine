#include "Component/CameraBoomComponent.h"

#include <cmath>
#include <cstring>

#include "Component/ActorComponent.h"
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
FVector NormalizeOrDefault(const FVector& Value, const FVector& Fallback)
{
    if (Value.LengthSquared() <= FMath::Epsilon * FMath::Epsilon)
    {
        return Fallback.Normalized();
    }

    return Value.Normalized();
}

float ComputeExpSmoothingAlpha(float Speed, float DeltaTime)
{
    if (Speed <= 0.0f || DeltaTime <= 0.0f)
    {
        return 1.0f;
    }

    return Clamp(1.0f - std::exp(-Speed * DeltaTime), 0.0f, 1.0f);
}
} // namespace

void UCameraBoomComponent::BeginPlay()
{
    USceneComponent::BeginPlay();

    ResolveTargetActor();
    bHasLastAppliedCameraLocation = false;
    UpdateCamera(0.0f);
}

void UCameraBoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    (void)ThisTickFunction;

    const bool bIsEditorTick = (TickType == LEVELTICK_ViewportsOnly);
    if (bIsEditorTick && !bUpdateInEditor)
    {
        return;
    }

    UpdateCamera(DeltaTime);
}

void UCameraBoomComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);

    Ar << TargetActorName;
    Ar << TargetDistance;
    Ar << OffsetDirection;

    Ar << TargetOffset;
    Ar << SocketOffset;
    Ar << bUpdateInEditor;
    Ar << bEnableCameraLag;
    Ar << CameraLagSpeed;
    Ar << bDoCollisionTest;
    Ar << ProbeSize;

    Ar << bEnableMouseOrbit;
    Ar << bRequireRightMouseForOrbit;
    Ar << MouseOrbitSensitivity;
    Ar << MinPitchDegrees;
    Ar << MaxPitchDegrees;

    if (Ar.IsLoading())
    {
        TargetActor = nullptr;
        bHasLastAppliedCameraLocation = false;
    }
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

    OutProps.push_back({ "Target Offset", EPropertyType::Vec3, &TargetOffset, 0.0f, 0.0f, 0.01f });

    OutProps.push_back({ "Socket Offset", EPropertyType::Vec3, &SocketOffset, 0.0f, 0.0f, 0.01f });

    OutProps.push_back({ "Update In Editor", EPropertyType::Bool, &bUpdateInEditor });

    OutProps.push_back({ "Enable Camera Lag", EPropertyType::Bool, &bEnableCameraLag });

    OutProps.push_back({ "Camera Lag Speed", EPropertyType::Float, &CameraLagSpeed, 0.0f, 100.0f, 0.1f });

    // OutProps.push_back({ "Do Collision Test", EPropertyType::Bool, &bDoCollisionTest });

    // OutProps.push_back({ "Probe Size", EPropertyType::Float, &ProbeSize, 0.0f, 100.0f, 0.1f });

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
        bHasLastAppliedCameraLocation = false;
    }

    if (std::strcmp(PropertyName, "Offset Direction") == 0)
    {
        OffsetDirection = NormalizeOrDefault(OffsetDirection, FVector(-1.0f, 0.0f, 0.35f));
        bHasLastAppliedCameraLocation = false;
    }

    if (std::strcmp(PropertyName, "Target Distance") == 0 ||
        std::strcmp(PropertyName, "Target Offset") == 0 ||
        std::strcmp(PropertyName, "Socket Offset") == 0 ||
        std::strcmp(PropertyName, "Enable Camera Lag") == 0 ||
        std::strcmp(PropertyName, "Camera Lag Speed") == 0)
        // std::strcmp(PropertyName, "Do Collision Test") == 0 ||
        // std::strcmp(PropertyName, "Probe Size") == 0)
    {
        bHasLastAppliedCameraLocation = false;
    }

    UpdateCamera(0.0f);
}

void UCameraBoomComponent::SetTargetActor(AActor* InTargetActor)
{
    TargetActor = InTargetActor;
    TargetActorName = InTargetActor ? InTargetActor->GetFName().ToString() : "None";
    bHasLastAppliedCameraLocation = false;
    UpdateCamera(0.0f);
}

void UCameraBoomComponent::UpdateCamera(float DeltaTime)
{
    ResolveTargetActor();

    UCameraComponent* Camera = FindChildCamera();
    if (!Camera)
    {
        return;
    }

    if (!TargetActor)
    {
        return;
    }

    const FVector PivotLocation = GetTargetPivotLocation();

    // Boom 자체를 Target Pivot 위치에 둡니다.
    // 이렇게 해야 에디터에서 Camera Actor의 루트 위치도 Target을 따라갑니다.
    SetWorldLocation(PivotLocation);

    FVector DesiredCameraLocation = ComputeDesiredCameraLocation(PivotLocation);
    // DesiredCameraLocation = ApplyCollisionTest(PivotLocation, DesiredCameraLocation);

    const FVector FinalCameraLocation = ApplyCameraLag(DesiredCameraLocation, DeltaTime);

    Camera->SetWorldLocation(FinalCameraLocation);
    Camera->LookAt(PivotLocation);
}

void UCameraBoomComponent::ResolveTargetActor()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        TargetActor = nullptr;
        return;
    }

    if (TargetActor && TargetActor->GetWorld() == World)
    {
        return;
    }

    if (TargetActorName.empty() || TargetActorName == "None")
    {
        TargetActor = GetOwner();
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        if (Actor->GetFName().ToString() == TargetActorName)
        {
            TargetActor = Actor;
            return;
        }
    }

    TargetActor = nullptr;
}

UCameraComponent* UCameraBoomComponent::FindChildCamera() const
{
    for (USceneComponent* Child : GetChildren())
    {
        if (UCameraComponent* Camera = Cast<UCameraComponent>(Child))
        {
            return Camera;
        }
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return nullptr;
    }

    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
        {
            return Camera;
        }
    }

    return nullptr;
}

FVector UCameraBoomComponent::GetTargetPivotLocation() const
{
    if (TargetActor)
    {
        return TargetActor->GetActorLocation() + TargetOffset;
    }

    return GetWorldLocation() + TargetOffset;
}

FVector UCameraBoomComponent::ComputeDesiredCameraLocation(const FVector& PivotLocation) const
{
    const FVector Direction = NormalizeOrDefault(OffsetDirection, FVector(-1.0f, 0.0f, 0.35f));
    return PivotLocation + Direction * TargetDistance + SocketOffset;
}

FVector UCameraBoomComponent::ApplyCameraLag(const FVector& DesiredCameraLocation, float DeltaTime)
{
    if (!bEnableCameraLag)
    {
        LastAppliedCameraLocationByBoom = DesiredCameraLocation;
        bHasLastAppliedCameraLocation = true;
        return DesiredCameraLocation;
    }

    if (!bHasLastAppliedCameraLocation)
    {
        LastAppliedCameraLocationByBoom = DesiredCameraLocation;
        bHasLastAppliedCameraLocation = true;
        return DesiredCameraLocation;
    }

    const float Alpha = ComputeExpSmoothingAlpha(CameraLagSpeed, DeltaTime);
    const FVector Result = LastAppliedCameraLocationByBoom +
                           (DesiredCameraLocation - LastAppliedCameraLocationByBoom) * Alpha;

    LastAppliedCameraLocationByBoom = Result;
    return Result;
}

FVector UCameraBoomComponent::ApplyCollisionTest(const FVector& PivotLocation, const FVector& DesiredCameraLocation) const
{
	// 일단 충돌 보정은 꺼둠
    (void)PivotLocation;

    return DesiredCameraLocation;
}
