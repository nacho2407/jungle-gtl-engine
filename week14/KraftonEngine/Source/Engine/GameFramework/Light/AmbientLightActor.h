#pragma once

#include "GameFramework/AActor.h"

class UAmbientLightComponent;
class UBillboardComponent;

class AAmbientLightActor : public AActor
{
public:
	DECLARE_CLASS(AAmbientLightActor, AActor)

	void InitDefaultComponents();

private:
	UAmbientLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
