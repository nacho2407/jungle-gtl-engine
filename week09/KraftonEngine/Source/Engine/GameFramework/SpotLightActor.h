#pragma once

#include "GameFramework/AActor.h"

class USpotLightComponent;
class UBillboardComponent;

class ASpotLightActor : public AActor
{
public:
	DECLARE_CLASS(ASpotLightActor, AActor)
	ASpotLightActor();

	void InitDefaultComponents();

private:
    USpotLightComponent* SpotLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString SpotLightIconPath = "Asset/Materials/SpotLightIcon.json";
};
