#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UPointLightComponent;
class UBillboardComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)
	APointLightActor();

	void InitDefaultComponents();

private:
    UPointLightComponent* PointLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString PointLightIconPath = FPaths::EditorRelativePath("Icons/Materials/PointLightIcon.json");
};
