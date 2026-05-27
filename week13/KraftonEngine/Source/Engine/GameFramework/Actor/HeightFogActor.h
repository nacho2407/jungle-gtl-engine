#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/HeightFogActor.generated.h"
class UHeightFogComponent;
class UBillboardComponent;

UCLASS()
class AHeightFogActor : public AActor
{
public:
	GENERATED_BODY()
	AHeightFogActor();
	void InitDefaultComponents();

	void PostDuplicate() override;

	UHeightFogComponent* GetFogComponent() const { return FogComponent; }


protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	UHeightFogComponent* FogComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
