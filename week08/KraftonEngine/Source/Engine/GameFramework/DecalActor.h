#pragma once

#include "GameFramework/AActor.h"

class UDecalComponent;
class UBillboardComponent;

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)

	ADecalActor();

	void InitDefaultComponents();

	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent;
	UBillboardComponent* BillboardComponent = nullptr;
	
	const FString DefaultDecalMaterialPath = "Asset/Materials/DefaultDecal.json";
	const FString DecalIconPath = "Asset/Materials/DecalIcon.json";
};
