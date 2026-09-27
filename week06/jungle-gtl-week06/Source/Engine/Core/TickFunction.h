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
    TG_PrePhysics = 0,
    TG_DuringPhysics,
    TG_PostPhysics,
    TG_PostUpdateWork,
    TG_MAX,
};

//TODO: Actor에 PrimaryTick을 구현해야함
struct FTickFunction
{
public:
    virtual ~FTickFunction() = default;

    virtual bool IsTargetValid() const = 0;
    virtual bool HasBegunPlay() const = 0;
    virtual void ExecuteTick(float DeltaTime, ELevelTick TickType) = 0;
	
    virtual const FString GetDebugName() const = 0;
protected:
    ETickingGroup TickGroup = TG_PrePhysics;      // 최소 실행 그룹

	// Tick 함수가 실행될 초(second) 단위 frequency
	float TickAccumulator = 0.0f;
	// Tick이 Manager에 등록되었는지 여부
	bool bRegistered = false;
	
public:
	bool bTickEvenWhenPaused	= false;
	bool bCanEverTick			= true; 
	bool bStartWithTickEnabled	= true;

	bool bTickEnabled			= true;
    bool bTickInEditor			= false;
	float TickInterval			= 0.0f; 

public:
	void RegisterTickFunction();
	void UnRegisterTickFunction();
	
	bool IsTickFunctionRegistered() const;
	bool CanTick(ELevelTick TickType) const;
	bool ConsumeInterval(float DeltaTime, float& OutTickDeltaTime);
	bool IsTickEnabled() const { return bTickEnabled && bCanEverTick; }
	ETickingGroup GetTickGroup() const{ return TickGroup; }

	void SetTickGroup(ETickingGroup InGroup) {TickGroup = InGroup;}
	void SetTickAccumulator(float InAccumulator) {TickAccumulator = (InAccumulator > 0.0f) ? InAccumulator : 0.0f; }
	float GetTickAccumulator() { return TickAccumulator; }

	void SetTickEnabled(bool bInEnabled) {
		bTickEnabled = bCanEverTick && bInEnabled;
	}
};

struct FActorTickFunction :public FTickFunction {
private:
    AActor* Target = nullptr;


public:
    void SetTarget(AActor* InTarget) { Target = InTarget; }

    bool IsTargetValid() const override;
    bool HasBegunPlay() const override;
	
    virtual void ExecuteTick(
        float DeltaTime,
        ELevelTick TickType) override;

	// FTickFunction을(를) 통해 상속됨
	const FString GetDebugName() const override;
};

struct FActorComponentTickFunction : public FTickFunction {
	UActorComponent* Target= nullptr;;
	
public:
	void SetTarget(UActorComponent* InTarget) { Target = InTarget; }

    bool IsTargetValid() const override;
    bool HasBegunPlay() const override;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType) override;

	// FTickFunction을(를) 통해 상속됨
	const FString GetDebugName() const override;
};

class FTickManager
{
public:
	void Tick(UWorld* World, float DeltaTime, ELevelTick TickType);
	void Reset();

private:
	void GatherTickFunctions(UWorld* World, ELevelTick TickType);
	void QueueTickFunction(FTickFunction& TickFunction);

	TArray<FTickFunction*> TickFunctionsByGroup[TG_MAX];
};
