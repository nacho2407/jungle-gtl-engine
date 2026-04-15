#pragma once

#include "GameFramework/AActor.h"

class UHeightFogComponent;
class UBillboardComponent;

class AHeightFogActor : public AActor
{
public:
	DECLARE_CLASS(AHeightFogActor, AActor)

	AHeightFogActor();
	void InitDefaultComponents();

	UHeightFogComponent* GetFogComponent() const { return FogComponent; }

private:
	UHeightFogComponent* FogComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
