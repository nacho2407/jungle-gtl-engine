#pragma once

#include "GameFramework/AActor.h"

class UPointLightComponent;
class UBillboardComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)
	APointLightActor();

	void InitDefaultComponents();

private:
    UPointLightComponent* PointLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString PointLightIconPath = "Asset/Materials/PointLightIcon.json";
};
