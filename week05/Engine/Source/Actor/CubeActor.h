#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class ENGINE_API ACubeActor : public AActor
{
public:
	DECLARE_RTTI(ACubeActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;


private:
	UStaticMeshComponent*CubeMeshComponent = nullptr;
};
