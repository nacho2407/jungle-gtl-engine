#pragma once

#include "CoreTypes.h"
#include "Component/USceneComponent.h"

class UCameraComponent : public USceneComponent
{
public:
    UCameraComponent();
    virtual ~UCameraComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual const char* GetObjClassName() const override;

public:
    void SetFieldOfView(float InFOV);
    float GetFieldOfView() const;

    void SetAspectRatio(float InAspectRatio);
    float GetAspectRatio() const;

    void SetNearClip(float InNearClip);
    float GetNearClip() const;

    void SetFarClip(float InFarClip);
    float GetFarClip() const;

    FMatrix GetProjectionMatrix() const;
    FMatrix GetViewMatrix() const;
    void UpdateAspectRatio(uint32 Width, uint32 Height);

private:
    float FieldOfView;
    float AspectRatio;
    float NearClip;
    float FarClip;
};

