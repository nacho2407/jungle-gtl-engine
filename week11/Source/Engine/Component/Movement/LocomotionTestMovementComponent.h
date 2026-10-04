#pragma once

#include "LocomotionTestMovementComponent.generated.h"

#include "MovementComponent.h"

UCLASS()
class ULocomotionTestMovementComponent : public UMovementComponent
{
    GENERATED_BODY_ULocomotionTestMovementComponent()
public:
    DECLARE_CLASS(ULocomotionTestMovementComponent, UMovementComponent)

    void TickComponent(float DeltaTime) override;

    float GetMoveSpeed() const { return MoveSpeed; }
    void SetMoveSpeed(float InMoveSpeed) { MoveSpeed = InMoveSpeed; }

    float GetTurnSpeed() const { return TurnSpeed; }
    void SetTurnSpeed(float InTurnSpeed) { TurnSpeed = InTurnSpeed; }

    float GetMaxSpeed() const override { return MoveSpeed; }

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    void ReadMoveInput(float& OutForwardAxis, float& OutTurnAxis) const;

private:
    UPROPERTY(EditAnywhere, Category="Movement", DisplayName="Move Speed")
    float MoveSpeed = 250.0f;

    UPROPERTY(EditAnywhere, Category="Movement", DisplayName="Turn Speed")
    float TurnSpeed = 180.0f;
};
