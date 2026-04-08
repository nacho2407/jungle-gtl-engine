#pragma once

#include "Actor.h"

class UTextRenderComponent;

class ENGINE_API ATextActor : public AActor
{
public:
	DECLARE_RTTI(ATextActor, AActor)

	void PostSpawnInitialize() override;

private:
	UTextRenderComponent* TextComponent = nullptr;
};