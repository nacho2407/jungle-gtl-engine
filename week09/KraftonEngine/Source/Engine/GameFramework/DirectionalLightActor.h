#pragma once

#include "GameFramework/AActor.h"

class UDirectionalLightComponent;
class UBillboardComponent;

class ADirectionalLightActor : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, AActor)
	ADirectionalLightActor();

	void InitDefaultComponents();

private:
    UDirectionalLightComponent* DirectionalLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString DirectionalLightIconPath = "Asset/Materials/DirectionalLightIcon.json";
};
