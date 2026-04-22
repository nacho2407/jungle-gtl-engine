#pragma once
#include "AActor.h"

class UDecalComponent;
class UStaticMeshComponent;

class AFireballActor : public AActor
{
public:
	DECLARE_CLASS(AFireballActor, AActor);
	AFireballActor();

	void InitDefaultComponents();
	
private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UDecalComponent* DecalComponents[3] = { nullptr, }; // xyz 각 방향으로 1개씩
	const FString FireballMeshName = "Data/BasicShape/Sphere.OBJ";
	const FString LightAreaMaterialPath = "Asset/Materials/FakeLight_LightArea.mat";
};
