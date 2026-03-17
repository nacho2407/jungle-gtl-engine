#pragma once

#include "Actor/AActor.h"
#include "FUObjectFactory.h"
#include "Component/UCameraComponent.h"

class ACamera : public AActor
{
public:
    ACamera();
    virtual ~ACamera() override;

//public:
//    virtual const char* GetObjClassName() const override;

    void SetAspectRatio(float Ratio);

public:
    UCameraComponent* GetCameraComponent() const;

private:
    void InitializeActor();

private:
    UCameraComponent* CameraComponent = nullptr;
};