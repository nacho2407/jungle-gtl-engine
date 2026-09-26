#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class ENGINE_API APlaneActor : public AActor
{
public:
	DECLARE_RTTI(APlaneActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	UStaticMeshComponent* PlaneMeshComponent = nullptr;
};
