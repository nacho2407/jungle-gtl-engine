#pragma once
#include "AActor.h"

class UDecalComponent;
class UStaticMeshComponent;

class AFireballActor : AActor
{
public:
	DECLARE_CLASS(AFireballActor, AActor);
	AFireballActor();

	void InitDefaultComponents();
	
private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UDecalComponent* DecalComponents[3] = { nullptr, }; // xyz 각 방향으로 1개씩
};
