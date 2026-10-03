#pragma once

#include "GameFramework/Pawn.h"

class UCameraComponent;

class ADefaultPawn : public APawn
{
public:
    DECLARE_CLASS(ADefaultPawn, APawn)
    ADefaultPawn() = default;

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;
    UCameraComponent* GetCameraComponent() const { return CameraComp; }

private:
    UCameraComponent* CameraComp = nullptr;

    float MoveSpeed = 15.0f;
    float FastMoveMultiplier = 4.0f;
    float LookSensitivityDegrees = 0.12f;
};
