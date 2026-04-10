#pragma once

#include "GameFramework/AActor.h"

class UDecalComponent;

class ADecalActor : public AActor
{
	public:
	DECLARE_CLASS(ADecalActor, AActor)
	ADecalActor() {};
	void InitDefaultComponents(const FString& UTextureFileName);

private:
	UDecalComponent* DecalComponent = nullptr;
};

