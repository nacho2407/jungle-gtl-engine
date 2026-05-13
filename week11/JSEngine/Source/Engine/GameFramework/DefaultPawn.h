#pragma once

#include "GameFramework/Pawn.h"

class UCameraComponent;

class ADefaultPawn : public APawn
{
public:
    DECLARE_CLASS(ADefaultPawn, APawn)
    ADefaultPawn() = default;

    void InitDefaultComponents() override;
    UCameraComponent* GetCameraComponent() const { return CameraComp; }

private:
    UCameraComponent* CameraComp = nullptr;
};
