#pragma once
#include "UObject.h"

class AActor;

class UActorComponent : public UObject
{
public:
	UActorComponent();
	virtual ~UActorComponent();
public:
	// Actor나 Component가 월드에 생성되고 게임이 시작된 직후 실행되는 '런타임 초기화 단계'
	virtual void BeginPlay();
	virtual void TickComponent(float DeltaTime);
	
public:
	void SetOwner(AActor* InOwner);
	AActor* GetOwner() const;
protected:
	AActor* Owner = nullptr; // 내가 어느 Actor에 속해있는지
};