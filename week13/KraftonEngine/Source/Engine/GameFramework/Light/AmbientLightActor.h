#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Light/AmbientLightActor.generated.h"
class UAmbientLightComponent;
class UBillboardComponent;

UCLASS()
class AAmbientLightActor : public AActor
{
public:
	GENERATED_BODY()
	void InitDefaultComponents();

	void PostDuplicate() override;


protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	UAmbientLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
