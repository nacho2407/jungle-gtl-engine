#pragma once

#include "Actor/AActor.h"
#include "FUObjectFactory.h"
#include "Component/UCameraComponent.h"

class ACamera : public AActor
{
    DECLARE_UClass(ACamera, AActor)

public:

    ACamera();
    virtual ~ACamera() override;

public:

    void SetAspectRatio(float Ratio);

public:
    UCameraComponent* GetCameraComponent() const;

private:
    void InitializeActor();

private:
    UCameraComponent* CameraComponent = nullptr;
    bool bUseOrthogonalProjection;
    float DebugOrthoWidth;
};
