#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UDecalComponent;
class UTextRenderComponent;

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)

	ADecalActor() = default;

	void InitDefaultComponents();
	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent = nullptr;
	UBillboardComponent* IconBillboardComponent = nullptr;
	UTextRenderComponent* TextRenderComponent = nullptr;
};
