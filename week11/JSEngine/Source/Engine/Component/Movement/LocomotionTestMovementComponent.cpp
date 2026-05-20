#include "LocomotionTestMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Input/GameplayInputTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Object/ObjectFactory.h"
#include "ReflectionSystem/ReflectionUtils.h"

#include <algorithm>

DEFINE_CLASS(ULocomotionTestMovementComponent, UMovementComponent)
REGISTER_FACTORY(ULocomotionTestMovementComponent)

void ULocomotionTestMovementComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);

    Ar << "MoveSpeed" << MoveSpeed;
    Ar << "TurnSpeed" << TurnSpeed;
}

void ULocomotionTestMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ReflectionUtils::AppendGeneratedPropertiesRecursive(this, GetStaticClass(), OutProps);

    OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed, 0.0f, 2000.0f, 1.0f });
    OutProps.push_back({ "Turn Speed", EPropertyType::Float, &TurnSpeed, 0.0f, 720.0f, 1.0f });
}

void ULocomotionTestMovementComponent::TickComponent(float DeltaTime)
{
    if (UpdatedComponent == nullptr && GetOwner())
    {
        UpdatedComponent = GetOwner()->GetRootComponent();
    }

    if (UpdatedComponent == nullptr || DeltaTime <= 0.0f)
    {
        Velocity = FVector::ZeroVector;
        return;
    }

    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (bUpdateOnlyIfRendered && PrimitiveComponent && !PrimitiveComponent->IsVisible())
    {
        Velocity = FVector::ZeroVector;
        return;
    }

    float ForwardAxis = 0.0f;
    float TurnAxis = 0.0f;
    ReadMoveInput(ForwardAxis, TurnAxis);

    if (TurnAxis != 0.0f)
    {
        FVector Rotation = UpdatedComponent->GetRelativeRotation();
        Rotation.Z += TurnAxis * TurnSpeed * DeltaTime;
        UpdatedComponent->SetRelativeRotation(Rotation);
    }

    FVector Forward = UpdatedComponent->GetRightVector();
    Forward.Z = 0.0f;
    if (!Forward.Normalize())
    {
        Forward = FVector(1.0f, 0.0f, 0.0f);
    }

    Velocity = Forward * (ForwardAxis * MoveSpeed);
    MoveUpdatedComponent(Velocity * DeltaTime);
}

void ULocomotionTestMovementComponent::ReadMoveInput(float& OutForwardAxis, float& OutTurnAxis) const
{
    OutForwardAxis = 0.0f;
    OutTurnAxis = 0.0f;

    const APawn* PawnOwner = Cast<APawn>(GetOwner());
    const APlayerController* PlayerController = PawnOwner ? PawnOwner->GetController() : nullptr;
    if (PlayerController)
    {
        if (const FInputActionState* MoveAction = PlayerController->FindInputAction("Move"))
        {
            OutTurnAxis = MoveAction->Value.Axis2D.X;
            OutForwardAxis = MoveAction->Value.Axis2D.Y;
        }
    }
    else
    {
        const InputSystem& Input = InputSystem::Get();
        OutForwardAxis =
            (Input.GetKey('W') ? 1.0f : 0.0f) -
            (Input.GetKey('S') ? 1.0f : 0.0f);
        OutTurnAxis =
            (Input.GetKey('D') ? 1.0f : 0.0f) -
            (Input.GetKey('A') ? 1.0f : 0.0f);
    }

    OutForwardAxis = std::clamp(OutForwardAxis, -1.0f, 1.0f);
    OutTurnAxis = std::clamp(OutTurnAxis, -1.0f, 1.0f);
}
