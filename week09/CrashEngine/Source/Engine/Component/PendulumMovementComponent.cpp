// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "PendulumMovementComponent.h"

#include "Component/SceneComponent.h"
#include "Math/MathUtils.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

#include "Input/InputSystem.h"

IMPLEMENT_COMPONENT_CLASS(UPendulumMovementComponent, UMovementComponent, EEditorComponentCategory::Movement)

void UPendulumMovementComponent::BeginPlay()
{
    UMovementComponent::BeginPlay();
    ElapsedTime = 0.0f;

    if (USceneComponent* Target = GetUpdatedComponent())
    {
        InitialRelativeRotation = Target->GetRelativeQuat();
    }
}

void UPendulumMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USceneComponent* Target = GetUpdatedComponent();
    if (!Target)
    {
        return;
    }

    ElapsedTime += DeltaTime;

    // angle = Amplitude * sin(2π * Frequency * t + Phase)
    const float PhaseRad = Phase * FMath::DegToRad;
    const float AngleDeg = Amplitude * std::sin(2.0f * FMath::Pi * Frequency * ElapsedTime + PhaseRad);

    // Axis 기준 절대 회전을 쿼터니언으로 설정
    const float AngleRad = (AngleDeg + AngleOffset) * FMath::DegToRad;
    FVector NormalizedAxis = Axis.Normalized();
    if (NormalizedAxis.Length() < 0.001f)
    {
        NormalizedAxis = FVector(0.0f, 1.0f, 0.0f);
    }

    FQuat SwingQuat = FQuat::FromAxisAngle(NormalizedAxis, AngleRad);

    FQuat FinalQuat = InitialRelativeRotation * SwingQuat;

    Target->SetRelativeRotation(FinalQuat);
    
    if (InputSystem::Get().GetSnapshot().KeyDown['D'])
    {
        Target->SetWorldLocation(Target->GetWorldLocation() + FVector(0.0f, 1.0f, 0.0f)*DeltaTime);
    }
}

void UPendulumMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Swing Axis", EPropertyType::Vec3, &Axis });
    OutProps.push_back({ "Amplitude (deg)", EPropertyType::Float, &Amplitude, 0.0f, 180.0f, 0.5f });
    OutProps.push_back({ "Frequency (Hz)", EPropertyType::Float, &Frequency, 0.01f, 10.0f, 0.01f });
    OutProps.push_back({ "Phase (deg)", EPropertyType::Float, &Phase, 0.0f, 360.0f, 1.0f });
    OutProps.push_back({ "Angle Offset (deg)", EPropertyType::Float, &AngleOffset, -180.0f, 180.0f, 0.5f });
}

void UPendulumMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << Axis.X;
    Ar << Axis.Y;
    Ar << Axis.Z;
    Ar << Amplitude;
    Ar << Frequency;
    Ar << Phase;
    Ar << AngleOffset;
}
