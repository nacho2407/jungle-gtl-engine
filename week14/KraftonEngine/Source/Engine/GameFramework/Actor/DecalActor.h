#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/DecalActor.generated.h"
class UTextRenderComponent;
class UDecalComponent;
class UBillboardComponent;

UCLASS()
class ADecalActor : public AActor
{
public:
	GENERATED_BODY()
	ADecalActor();

	void InitDefaultComponents();

	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent;
	UBillboardComponent* BillboardComponent = nullptr;
	UTextRenderComponent* TextRenderComponent = nullptr;
	
	const FString DefaultDecalMaterialPath = "Content/Material/Editor/DefaultDecal.mat";
};
