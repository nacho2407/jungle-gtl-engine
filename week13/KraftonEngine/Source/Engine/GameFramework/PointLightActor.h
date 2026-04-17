#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UPointLightComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)

	void InitDefaultComponents();

private:
	UPointLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
