#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/HeightFogActor.generated.h"
class UHeightFogComponent;
class UBillboardComponent;

UCLASS()
class AHeightFogActor : public AActor
{
public:
	GENERATED_BODY()
	AHeightFogActor();
	void InitDefaultComponents();

	UHeightFogComponent* GetFogComponent() const { return FogComponent; }

private:
	UHeightFogComponent* FogComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
