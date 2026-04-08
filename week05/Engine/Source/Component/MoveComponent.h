#pragma once
#include "SceneComponent.h"

class ENGINE_API UMoveComponent : public USceneComponent
{
public:
	DECLARE_RTTI(UMoveComponent, USceneComponent)

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	float Speed = 1.0f;
	float Height = 1.0f;
	float RotateSpeed = 7.0f;

	float ElapsedTime = 0.0f;
	FVector InitialPosition;
};
