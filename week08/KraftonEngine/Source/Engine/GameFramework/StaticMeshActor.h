#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComp;
class UTextRenderComponent;
class USubUVComponent;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents(const FString& UStaticMeshFileName);

private:
	UStaticMeshComp* StaticMeshComponent = nullptr;
	UTextRenderComponent* TextRenderComponent = nullptr;
	USubUVComponent* SubUVComponent = nullptr;
};