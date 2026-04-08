#pragma once
#include "SceneComponent.h"

class FCamera;

class ENGINE_API UCameraComponent : public USceneComponent
{
public:
	DECLARE_RTTI(UCameraComponent, USceneComponent)
	virtual ~UCameraComponent();

	void PostConstruct() override;
	virtual void Tick(float DeltaTime) override;
	//Movement method
	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveUp(float Value);
	void Rotate(float DeltaYaw, float DeltaPitch);

	//Camera property getter
	FCamera* GetCamera() const;
	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;

	//Setting
	void SetFov(float inFov);
	void SetSpeed(float Inspeed);
	void SetSensitivity(float InSetSensitivity);
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
private:
	FCamera* Camera = nullptr;
};
