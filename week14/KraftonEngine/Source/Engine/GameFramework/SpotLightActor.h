#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class USpotLightComponent;

class ASpotLightActor : public AActor
{
public:
	DECLARE_CLASS(ASpotLightActor, AActor)

	void InitDefaultComponents();

private:
	USpotLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
