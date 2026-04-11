#pragma once

#include "GameFramework/AActor.h"

class UTextRenderComponent;
class UDecalComponent;

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)

	ADecalActor();

	void InitDefaultComponents();

	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent;
	UTextRenderComponent* TextRenderComponent = nullptr;
};
