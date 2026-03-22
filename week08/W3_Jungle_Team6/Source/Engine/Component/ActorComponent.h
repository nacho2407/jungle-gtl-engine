#pragma once

#include "Object/Object.h"

class AActor;

class UActorComponent : public UObject
{
public:
	DECLARE_CLASS(UActorComponent, UObject)

	virtual void BeginPlay();
	virtual void EndPlay() {};

	virtual void Activate();
	virtual void Deactivate();

	void ExecuteTick(float DeltaTime);
	void SetActive(bool bNewActive);
	inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
	inline void SetComponentTickEnabled(bool bEnabled) { bCanEverTick = bEnabled; }

	inline bool IsActive() { return bIsActive; }

	void SetOwner(AActor* Actor) { Owner = Actor; }
	AActor* GetOwner() const { return Owner; }

protected:
	virtual void TickComponent(float DeltaTime) {}

protected:
	AActor* Owner = nullptr;

private:
	bool bIsActive = true;
	bool bAutoActivate = true;
	bool bCanEverTick = true;
};




