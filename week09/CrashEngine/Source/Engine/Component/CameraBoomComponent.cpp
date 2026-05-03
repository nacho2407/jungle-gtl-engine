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


void UCameraBoomComponent::BeginPlay()
{
    USceneComponent::BeginPlay();
}

void UCameraBoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    (void)TickType;
    (void)ThisTickFunction;
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
    }
}

void UCameraBoomComponent::SetTargetActor(AActor* InTargetActor)
{
    TargetActor = InTargetActor;
}
