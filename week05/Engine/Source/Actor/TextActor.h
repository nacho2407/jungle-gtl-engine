#pragma once

#include "Actor.h"

class UTextRenderComponent;

class ENGINE_API ATextActor : public AActor
{
public:
	DECLARE_RTTI(ATextActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	UTextRenderComponent* TextComponent = nullptr;
};

