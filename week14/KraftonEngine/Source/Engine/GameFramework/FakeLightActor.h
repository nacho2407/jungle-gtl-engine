#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComponent;
class UCylindricalBillboardComponent;
class UDecalComponent;;

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
	FString LampMeshDir = "Data/Retro-light/RetroLight.OBJ";
	FString LampshadeMaterialPath = "Asset/Materials/Lampshade.json";
	FString DecalMaterialPath = "Asset/Materials/FakeLight_LightArea.json";
};
