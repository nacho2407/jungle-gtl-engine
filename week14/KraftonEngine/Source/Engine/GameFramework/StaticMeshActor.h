#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USubUVComponent;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() {}

	void InitDefaultComponents(const FString& UStaticMeshFileName);

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UTextRenderComponent* TextRenderComponent = nullptr;
	USubUVComponent* SubUVComponent = nullptr;
};