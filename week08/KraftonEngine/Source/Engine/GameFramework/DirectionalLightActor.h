#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UDirectionalLightComponent;
class UBillboardComponent;

class ADirectionalLightActor : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLightActor, AActor)
	ADirectionalLightActor();

	void InitDefaultComponents();

private:
    UDirectionalLightComponent* DirectionalLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	FString DirectionalLightIconPath = FPaths::EditorRelativePath("Icons/Materials/DirectionalLightIcon.json");
};
