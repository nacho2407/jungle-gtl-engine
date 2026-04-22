#pragma once
#include "GameFramework/AActor.h"

class UBillboardComponent;
class UDirectionalLightComponent;

class ADirectionalLightActor : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, AActor)

	void InitDefaultComponents();

private:
	UDirectionalLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
