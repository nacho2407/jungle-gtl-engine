#pragma once

#include "GameFramework/AActor.h"

class UCylindricalBillboardComponent;
class UDecalComponent;;

class AFakeLightActor : public AActor
{
public:
	DECLARE_CLASS(AFakeLightActor, AActor)
	AFakeLightActor();

	void InitDefaultComponents();

private:
	UCylindricalBillboardComponent* BillboardComponent = nullptr;
	UDecalComponent* DecalComponent = nullptr;
	
	// TODO: Remove Magic Numbers
	FString LampshadeImage = "FakeLight_Lampshade";
	FString DecalMaterialPath = "Asset/Materials/FakeLight_LightArea.json";
};
