#pragma once
#include "AActor.h"
#include "Platform/Paths.h"

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
	const FString FireballMeshName = FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ");
	const FString LightAreaMaterialPath = FPaths::ContentRelativePath("Materials/FakeLight_LightArea.json");
};
