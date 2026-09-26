#pragma once
#include "Actor.h"

class UBillboardComponent;

class ENGINE_API ABillboardActor : public AActor
{
public:
	DECLARE_RTTI(ABillboardActor, AActor)

	void PostSpawnInitialize() override;

private:
	UBillboardComponent* BillboardComponent = nullptr;
};
