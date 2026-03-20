#pragma once

#include "Object/Object.h"

class AActor;

class UActorComponent : public UObject
{
protected:
	virtual void TickComponent(float DeltaTime) {};
	AActor* OwningActor = nullptr;

private:
	bool bIsActive = true;
	bool bAutoActivate = true;
	bool bCanEverTick = true;

public:
	DECLARE_CLASS(UActorComponent, UObject)
	virtual void BeginPlay();
	virtual void EndPlay() {};

	virtual void Activate();
	virtual void Deactivate();
	
	void ExcuteTick(float DeltaTime);
	void SetActive(bool bNewActive);
	inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
	inline void SetComponentTickEnabled(bool bEnabled) { bCanEverTick = bEnabled; }


	inline bool IsActive() { return bIsActive; }

	void SetOwningActor(AActor* Actor) { OwningActor = Actor; }
	AActor* GetOwningActor() const { return OwningActor; }
};




