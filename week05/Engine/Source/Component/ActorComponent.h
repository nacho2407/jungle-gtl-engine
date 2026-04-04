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
	// 렌더 루프 전용. GC가 실행되지 않는 프레임 내에서 해시맵 조회 없이 포인터를 반환한다.
	AActor* GetOwnerFast() const { return Owner.GetUnchecked(); }
	void SetOwner(AActor* InOwner) { Owner = InOwner; }

	bool IsRegistered() const { return bRegistered; }
	virtual void OnRegister() { bRegistered = true; }
	virtual void OnUnregister() { bRegistered = false; }
	virtual void BeginPlay() { bBegunPlay = true; }
	virtual void Tick(float DeltaTime) {}
	bool HasBegunPlay() const { return bBegunPlay; }
	bool CanTick() const { return bCanEverTick && bTickEnabled; }
	void SetComponentTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }

	virtual void Serialize(FArchive& Ar);

protected:
	TObjectPtr<AActor> Owner;
	bool bRegistered = false;
	bool bBegunPlay = false;
	bool bCanEverTick = false;
	bool bTickEnabled = true;
};

