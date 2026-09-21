#pragma once
#include "UObject.h"

class AActor;

class UActorComponent : public UObject
{
public:
	DECLARE_UClass(UActorComponent, UObject)

	UActorComponent();
	virtual ~UActorComponent();

public:
	// Actor나 Component가 월드에 생성되고 게임이 시작된 직후 실행되는 '런타임 초기화 단계'
	virtual void BeginPlay();
	virtual void TickComponent(float DeltaTime);

	// 활성화/비활성화
	virtual void Activate();
	virtual void Deactivate();

	bool CanEverTick() const { return bCanEverTick; }
	void SetCanEverTick(bool bInCanEverTick) { bCanEverTick = bInCanEverTick; }

	bool IsActive() const { return bIsActivate; }

	bool HasBegunPlay() const { return bHasBegunPlay; }

public:
	void SetOwner(AActor* InOwner);
	AActor* GetOwner() const;
protected:
	AActor* Owner = nullptr; // 내가 어느 Actor에 속해있는지

	bool bCanEverTick = true;      // Tick을 켤지 끌지 결정
	bool bIsActivate = true;       // 현재 활성화 여부
	bool bHasBegunPlay = false;    // BeginPlay가 호출되었는지 여부
};