#pragma once
#include "GameFramework/StaticMeshActor.h"

class UProjectileMovementComponent;

class AEggActor : public AStaticMeshActor 
{
public:
	DECLARE_CLASS(AEggActor, AStaticMeshActor)
	void InitDefaultComponents();
private:
	UProjectileMovementComponent* ProjectileComponent;
};

