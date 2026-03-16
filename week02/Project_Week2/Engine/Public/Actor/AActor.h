#pragma once

#include <vector>
#include "UObject.h"
#include "TObjectBase.h"

class UActorComponent;
class USceneComponent;

class AActor : public UObject
{
CreateMetaData(AActor)
public:
	AActor() : UObject()
	{
		RootComponent = nullptr;
	}
	AActor(const FUObjectInitializer& ObjectInitializer);
	virtual ~AActor();
public:
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);

public:
	void AddComponent(UActorComponent* InComponent);

	const std::vector<UActorComponent*>& GetComponents() const;

	void SetRootComponent(USceneComponent* InRootComponent);
	USceneComponent* GetRootComponent() const;
protected:
	std::vector<UActorComponent*> Components;
	USceneComponent* RootComponent;
};