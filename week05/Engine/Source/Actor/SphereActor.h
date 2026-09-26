#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class ENGINE_API ASphereActor : public AActor
{
public:
	DECLARE_RTTI(ASphereActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	UStaticMeshComponent* SphereMeshComponent = nullptr;
};
