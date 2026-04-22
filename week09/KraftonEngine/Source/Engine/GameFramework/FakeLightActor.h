#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UStaticMeshComponent;
class UCylindricalBillboardComponent;
class UDecalComponent;

class AFakeLightActor : public AActor
{
public:
	DECLARE_CLASS(AFakeLightActor, AActor)
	AFakeLightActor();

	void InitDefaultComponents();

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UCylindricalBillboardComponent* BillboardComponent = nullptr;
	UDecalComponent* DecalComponent = nullptr;
	
	// TODO: Remove Magic Numbers
	FString LampMeshDir = FPaths::ContentRelativePath("Models/Retro-light/RetroLight.OBJ");
	FString LampshadeMaterialPath = FPaths::ContentRelativePath("Materials/Lampshade.json");
	FString DecalMaterialPath = FPaths::ContentRelativePath("Materials/FakeLight_LightArea.json");
};
