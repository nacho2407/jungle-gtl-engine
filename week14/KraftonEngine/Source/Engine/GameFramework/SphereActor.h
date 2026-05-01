#pragma once

#include "GameFramework/AActor.h"

class USphereComponent;

class ASphereActor : public AActor
{
public:
	DECLARE_CLASS(ASphereActor, AActor)

	ASphereActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;
	void BeginPlay() override;

	USphereComponent* GetSphereComponent() const { return SphereComponent; }

private:
	USphereComponent* SphereComponent = nullptr;
};
