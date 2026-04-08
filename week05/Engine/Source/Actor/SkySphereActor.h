#pragma once

#include "Actor.h"

class USkyComponent;

class ENGINE_API ASkySphereActor : public AActor
{
public:
	DECLARE_RTTI(ASkySphereActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	USkyComponent* SkySphereComponent = nullptr;
};
