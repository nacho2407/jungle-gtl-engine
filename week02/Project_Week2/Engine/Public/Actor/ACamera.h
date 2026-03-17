#pragma once

#include "Actor/AActor.h"

class UCameraComponent;

class ACamera : public AActor
{
DECLARE_ROOT_UClass(AActor)

public:

    ACamera();
    ACamera(const FUObjectInitializer& ObjectInitializer);
    virtual ~ACamera() override;

public:
    UCameraComponent* GetCameraComponent() const;

private:
    void InitializeActor();

private:
    UCameraComponent* CameraComponent = nullptr;
};
