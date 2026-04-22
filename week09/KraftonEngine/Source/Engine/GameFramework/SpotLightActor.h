#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class USpotLightComponent;
class UBillboardComponent;

class ASpotLightActor : public AActor
{
public:
	DECLARE_CLASS(ASpotLightActor, AActor)
	ASpotLightActor();

	void InitDefaultComponents();

private:
    USpotLightComponent* SpotLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString SpotLightIconPath = FPaths::EditorRelativePath("Icons/Materials/SpotLightIcon.json");
};
