#pragma once

#include "Core/CoreTypes.h"

class AActor;
class UActorComponent;
class UWorld;

enum ELevelTick : int
{
	LEVELTICK_All,
	LEVELTICK_ViewportsOnly,
	LEVELTICK_TimeOnly,
	LEVELTICK_PauseTick,
};

enum ETickingGroup : int
{
    TG_PrePhysics,
    TG_DuringPhysics,
    TG_PostPhysics,
    TG_PostUpdateWork,
    TG_MAX,
};

//TODO: Actor에 PrimaryTick을 구현해야함
struct FTickFunction
{
    ETickingGroup TickGroup = TG_PrePhysics;      // 최소 실행 그룹
    ETickingGroup EndTickGroup = TG_PrePhysics;   // 완료 보장 그룹

	// Tick 함수가 실행될 초(second) 단위 frequency
	float TickInterval = 0.0f; 
	float TickAccumulator = 0.0f;
	
public:
	//tickFunction에 들어가야하는 변수들
	// Pause시 Tick을 돌리는지 여부
	bool bTickEvenWhenPaused = false;
	// 틱으로 절대 등록하지 않음
	bool bCanEverTick = false; 
	// BeginPlay이후부터 바로 Tick함수 실행
	bool bStartWithTickEnabled = true;
	bool bRegistered = false;

	//현재상태 변수
	//현재 틱을 사용할건지 여부
	bool bTickEnabled  = true;

public:
    virtual ~FTickFunction() = default;

	void SetTickGroup(ETickingGroup InGroup) {TickGroup = InGroup;}
	void SetEndTickGroup(ETickingGroup InGroup) {EndTickGroup = InGroup;}
	void SetTickInterval(float InInterval) {TickInterval = (InInterval > 0.0f) ? InInterval : 0.0f; }
	
	ETickingGroup GetTickGroup() const { return TickGroup; }
	ETickingGroup GetEndTickGroup() const { return EndTickGroup; }
	float GetTickInterval() const { return TickInterval; }
	
    float& GetTickAccumulator() { return TickAccumulator; }

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType) = 0;
	virtual const char* GetDebugName() const = 0;

	void RegisterTickFunction();
	void UnRegisterTickFunction();

	bool ConsumeInterval(float DeltaTime)
	{
		if (TickInterval <= 0.0f)
		{
			return true;
		}

		TickAccumulator += DeltaTime;
		if (TickAccumulator < TickInterval)
		{
			return false;
		}

		TickAccumulator -= TickInterval;
		return true;
	}

	void SetTickEnabled(bool bInEnabled)
	{
		bTickEnabled = bInEnabled;
	}

	void ResetInterval()
	{
		TickAccumulator = 0.0f;
	}

	bool CanTick(ELevelTick TickType) const
	{
		if (!bCanEverTick || !bTickEnabled || !bRegistered)
		{
			return false;
		}

		if (TickType == LEVELTICK_PauseTick && !bTickEvenWhenPaused)
		{
			return false;
		}

		return true;
	}
};

class FTickManager
{
public:
	void Tick(UWorld* World, float DeltaTime, ELevelTick TickType);
	void Reset();

private:
	void GatherTickFunctions(UWorld* World, ELevelTick TickType);
	void QueueTickFunction(FTickFunction& TickFunction);

	TArray<FTickFunction*> TickFunctions;
};

struct FActorTickFunction :public FTickFunction {
private:
    AActor* Target = nullptr;

public:
    void SetTarget(AActor* InTarget) { Target = InTarget; }

    virtual void ExecuteTick(
        float DeltaTime,
        ELevelTick TickType) override;


	// FTickFunction을(를) 통해 상속됨
	const char* GetDebugName() const override;
};

struct FActorComponentTickFunction : public FTickFunction {
	UActorComponent* Target= nullptr;;
	
public:
	void SetTarget(UActorComponent* InTarget) { Target = InTarget; }
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType) override;

	// FTickFunction을(를) 통해 상속됨
	const char* GetDebugName() const override;
};
