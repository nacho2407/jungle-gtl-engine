#pragma once

#include "Actor.h"

class UTextComponent;

class ENGINE_API ATextActor : public AActor
{
public:
	DECLARE_RTTI(ATextActor, AActor)

	void PostSpawnInitialize() override;

	UTextComponent* GetTextComponent() const { return TextComponent; }

private:
	UTextComponent* TextComponent = nullptr;
};