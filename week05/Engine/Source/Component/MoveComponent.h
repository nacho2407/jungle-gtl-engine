#pragma once
#include "ActorComponent.h"

class ENGINE_API UMoveComponent : public UActorComponent
{
public:
	DECLARE_RTTI(UMoveComponent, UActorComponent)

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	float Speed = 1.0f;
	float Height = 1.0f;
	float RotateSpeed = 7.0f;

	float ElapsedTime = 0.0f;
	FVector InitialPosition;
};
