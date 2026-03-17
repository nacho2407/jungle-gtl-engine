#pragma once

#include <vector>

#include "UObject.h"
#include "TObjectBase.h"
#include "CoreTypes.h"

class UActorComponent;
class USceneComponent;

class AActor : public UObject
{
DECLARE_UClass(AActor, UObject)

public:
	AActor() : UObject()
	{
		RootComponent = nullptr;
	}

	virtual ~AActor();
public:
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);

public:
	void AddComponent(UActorComponent* InComponent);

	const TArray<UActorComponent*>& GetComponents() const;

	void SetRootComponent(USceneComponent* InRootComponent);
	USceneComponent* GetRootComponent() const;

protected:
	TArray<UActorComponent*> Components;
	USceneComponent* RootComponent;
};