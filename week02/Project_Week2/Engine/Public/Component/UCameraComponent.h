#pragma once

#include "CoreTypes.h"
#include "Component/USceneComponent.h"

enum class EProjectionMode
{
    Perspective = 0,
	Orthogonal
};

class UCameraComponent : public USceneComponent
{
    DECLARE_UClass(UCameraComponent, USceneComponent)

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

    void SetProjectionMode(EProjectionMode InMode);
    EProjectionMode GetProjectionMode() const;

    void SetOrthoWidth(float InOrthoWidth);
    float GetOrthoWidth() const;

    bool IsOrthogonal() const;


    FMatrix GetProjectionMatrix() const;
    FMatrix GetViewMatrix() const;
    void UpdateAspectRatio(uint32 Width, uint32 Height);

private:
    float FieldOfView;
    float AspectRatio;
    float NearClip;
    float FarClip;

    EProjectionMode ProjectionMode = EProjectionMode::Perspective;;
    float OrthoWidth = 10.0f;
};

