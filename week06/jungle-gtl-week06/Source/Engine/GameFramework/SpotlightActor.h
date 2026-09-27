#pragma once
#include "GameFramework/AActor.h"

class UBillboardComponent;
class UDecalComponent;
class UTextRenderComponent;

class ASpotlightActor :
    public AActor
{
public:
	DECLARE_CLASS(ADecalASpotlightActorActor, AActor)

	ASpotlightActor() = default;

	void InitDefaultComponents();
	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	UTextRenderComponent* TextRenderComponent = nullptr;
};

