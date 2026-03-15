#pragma once

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

private:
    float FieldOfView;
    float AspectRatio;
    float NearClip;
    float FarClip;
};


	//FMatrix GetProjectionMatrix() const { return ProjectionMatrix; } 이것도 여기서 하는 게 좋지 않나?
	//FMatrix GetViewMatrix() const { return ViewMatrix; } 이거 들어가야 하지 않나?
//void UpdateAspectRatio(uint32 Width, uint32 Height); // 창 크기 변경시에 출력