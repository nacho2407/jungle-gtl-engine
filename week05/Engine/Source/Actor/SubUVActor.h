#pragma once
#pragma once

#include "Actor.h"

class USubUVComponent;

class ENGINE_API ASubUVActor : public AActor
{
public:
	DECLARE_RTTI(ASubUVActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	USubUVComponent* SubUVComponent = nullptr;
};
