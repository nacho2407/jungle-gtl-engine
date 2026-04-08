#pragma once

#include "Actor.h"

class UTextComponent;

class ENGINE_API ATextActor : public AActor
{
public:
	DECLARE_RTTI(ATextActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	UTextComponent* TextComponent = nullptr;
};
