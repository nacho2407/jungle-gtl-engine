#pragma once

#include "Actor/AActor.h"

class UCameraComponent;

class ACamera : public AActor
{
public:
    ACamera();
    ACamera(const FUObjectInitializer& ObjectInitializer);
    virtual ~ACamera() override;

public:
    virtual const char* GetObjClassName() const override;

public:
    UCameraComponent* GetCameraComponent() const;

private:
    void InitializeActor();

private:
    UCameraComponent* CameraComponent = nullptr;
};