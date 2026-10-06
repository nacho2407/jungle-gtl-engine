#pragma once
#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Light/DirectionalLightActor.generated.h"
class UBillboardComponent;
class UDirectionalLightComponent;

UCLASS()
class ADirectionalLightActor : public AActor
{
public:
	GENERATED_BODY()
	void InitDefaultComponents();

	void PostDuplicate() override;


protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	UDirectionalLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
