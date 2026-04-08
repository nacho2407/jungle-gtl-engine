#pragma once
#include "Object/Object.h"

class FArchive;
class AActor;

class ENGINE_API UActorComponent : public UObject
{
public:
	DECLARE_RTTI(UActorComponent, UObject)

	~UActorComponent() override = default;

	AActor* GetOwner() const { return Owner; }
	void SetOwner(AActor* InOwner) { Owner = InOwner; }

	bool IsRegistered() const { return bRegistered; }
	virtual void OnRegister() { bRegistered = true; }
	virtual void OnUnregister() { bRegistered = false; }
	virtual void BeginPlay() { bBegunPlay = true; }
	virtual void Tick(float DeltaTime) {}
	bool HasBegunPlay() const { return bBegunPlay; }
	bool IsComponentTickEnabled() const { return bTickEnabled; }
	bool IsTickInEditor() const { return bTickInEditor; }
	bool CanTick() const { return bCanEverTick && bTickEnabled; }
	void SetComponentTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
	void SetTickInEditor(bool bEnabled) { bTickInEditor = bEnabled; }
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

	virtual void Serialize(FArchive& Ar);

protected:
	TObjectPtr<AActor> Owner;
	bool bRegistered = false;
	bool bBegunPlay = false;
	bool bCanEverTick = false;
	bool bTickEnabled = true;
	bool bTickInEditor = false;
};

