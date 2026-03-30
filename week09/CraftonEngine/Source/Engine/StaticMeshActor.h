#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComp;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents(const FString& UStaticMeshFileName);

private:
	UStaticMeshComp* StaticMeshComponent;
};