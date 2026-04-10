#pragma once

#include "GameFramework/AActor.h"

class UDecalComponent;

class ADecalActor : public AActor
{
	public:
	DECLARE_CLASS(ADecalActor, AActor)
	ADecalActor() {};
	void InitDefaultComponents();

private:
	UDecalComponent* DecalComponent = nullptr;
};

