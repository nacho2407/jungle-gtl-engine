#pragma once
#include "CoreMinimal.h"
#include "Actor/Actor.h"

class UStaticMeshComponent;
class URandomColorComponent;

class ENGINE_API AStaticMeshActor : public AActor
{
public:
	DECLARE_RTTI(AStaticMeshActor, AActor)

	virtual void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	UStaticMeshComponent* StaticMeshComp = nullptr;
};

