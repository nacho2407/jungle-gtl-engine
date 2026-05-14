#include "GameFramework/DefaultPawn.h"

#include "Component/CameraComponent.h"
#include "Engine/Input/GameplayInputTypes.h"
#include "GameFramework/PlayerController.h"

DEFINE_CLASS(ADefaultPawn, APawn)
REGISTER_FACTORY(ADefaultPawn)

namespace
{
    bool IsActionActive(const FInputActionState* Action)
    {
        return Action &&
            (Action->TriggerEvent == EInputTriggerEvent::Started ||
             Action->TriggerEvent == EInputTriggerEvent::Triggered);
    }
}

void ADefaultPawn::InitDefaultComponents()
{
    CameraComp = AddComponent<UCameraComponent>();
    SetRootComponent(CameraComp);
    AddTag("DefaultPawn");
}

void ADefaultPawn::Tick(float DeltaTime)
{
    APawn::Tick(DeltaTime);

    if (!CameraComp || DeltaTime <= 0.0f)
    {
        return;
    }

    APlayerController* PlayerController = GetController();
    if (!PlayerController || !PlayerController->IsMouseCaptured())
    {
        return;
    }

    const FGameplayInputSnapshot& Snapshot = PlayerController->GetInputSnapshot();
    const bool bFastMove = IsActionActive(Snapshot.FindAction("Dash"));
    const float EffectiveMoveSpeed = MoveSpeed * (bFastMove ? FastMoveMultiplier : 1.0f);
    const float MoveDistance = EffectiveMoveSpeed * DeltaTime;

    if (const FInputActionState* MoveAction = Snapshot.FindAction("Move"))
    {
        const FVector2& MoveAxis = MoveAction->Value.Axis2D;
        CameraComp->MoveForward(MoveAxis.Y * MoveDistance);
        CameraComp->MoveRight(MoveAxis.X * MoveDistance);
    }

    if (const FInputActionState* MoveVerticalAction = Snapshot.FindAction("MoveVertical"))
    {
        CameraComp->MoveUp(MoveVerticalAction->Value.Axis1D * MoveDistance);
    }

    if (const FInputActionState* LookAction = Snapshot.FindAction("Look"))
    {
        const FVector2& LookAxis = LookAction->Value.Axis2D;
        CameraComp->AddYawInput(LookAxis.X * LookSensitivityDegrees);
        CameraComp->AddPitchInput(-LookAxis.Y * LookSensitivityDegrees);
    }
}
