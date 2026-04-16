#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

void FTickFunction::RegisterTickFunction()
{
	bRegistered = true;
	TickAccumulator = 0.0f;
}

void FTickFunction::UnRegisterTickFunction()
{
	bRegistered = false;
	TickAccumulator = 0.0f;
}


bool FTickFunction::ConsumeInterval(float DeltaTime, float& OutTickDeltaTime)
{
	if (TickInterval <= 0.0f)
	{
		OutTickDeltaTime = DeltaTime;
		return true;
	}
	
	TickAccumulator += DeltaTime;
	if (TickAccumulator < TickInterval)
	{
		return false;
	}
	
    OutTickDeltaTime = TickAccumulator;
    TickAccumulator = 0.0f;
	return true;
}

bool FTickFunction::IsTickFunctionRegistered() const
{
	return bRegistered;
}

bool FTickFunction::CanTick(ELevelTick TickType) const
{
    // 1. 기본 유효성 검사 + 나중에 Pendkill관리도 포함해야함
    if (!IsTargetValid())
    {
        return false;
    }

	if(!IsTickEnabled()) return false;

    switch (TickType)
    {
    case LEVELTICK_All:
        // 일반 게임 플레이 — BeginPlay 완료된 것만
        return HasBegunPlay();
 
    case LEVELTICK_ViewportsOnly:
        // 에디터 — bTickInEditor 플래그만 확인
        return bTickInEditor;
	case LEVELTICK_TimeOnly:
		// 일시정지 중 시간만 진행
        // 타이머·딜레이 등 엔진 내부 시간 시스템만 이 타입을 소비함
        return false;
	case LEVELTICK_PauseTick:
        // 일시정지 중 예외적으로 Tick이 허용된 것만
        // BeginPlay는 완료돼 있어야 하고, bTickEvenWhenPaused도 true여야 함
		return HasBegunPlay() && bTickEvenWhenPaused;
 
    default:
        return false;
	}
}

// =======================================================================
// FActorTickFunction
// =======================================================================

bool FActorTickFunction::IsTargetValid() const
{
    return Target != nullptr && !Target->IsPendingDestroy();
}

bool FActorTickFunction::HasBegunPlay() const
{
    return Target && Target->HasActorBegunPlay();
}

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickActor(DeltaTime, TickType, *this);
	}
}

const FString FActorTickFunction::GetDebugName() const
{
	return Target ? Target->GetTypeInfo()->name : "FActorTickFunction";
}

// =======================================================================
// FActorComponentTickFunction
// =======================================================================
 
bool FActorComponentTickFunction::IsTargetValid() const
{
    return Target != nullptr && !Target->IsPendingDestroy();
}

bool FActorComponentTickFunction::HasBegunPlay() const
{
    if (!Target) { return false; }
    AActor* Owner = Target->GetOwner();
    return Owner && Owner->HasActorBegunPlay();
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickComponent(DeltaTime, TickType, *this);
	}
}

const FString FActorComponentTickFunction::GetDebugName() const
{
	return Target ? Target->GetTypeInfo()->name : "FActorComponentTickFunction";
}

// =======================================================================
// FTickManager
// =======================================================================

void FTickManager::Tick(UWorld* World, float DeltaTime, ELevelTick TickType)
{
	GatherTickFunctions(World, TickType);

	for (int GroupIndex = 0; GroupIndex < TG_MAX; ++GroupIndex)
	{
		TArray<FTickFunction*>& GroupTicks = TickFunctionsByGroup[GroupIndex];

		for (FTickFunction* TickFunction : GroupTicks)
		{
			if (!TickFunction)
			{
				continue;
			}

			if (!TickFunction->CanTick(TickType))
			{
				continue;
			}

			float totalTick = 0.0f;
			if (!TickFunction->ConsumeInterval(DeltaTime, totalTick))
			{
				continue;
			}

			TickFunction->ExecuteTick(totalTick, TickType);
		}
	}
}

void FTickManager::Reset()
{
	for (TArray<FTickFunction*>& GroupTicks : TickFunctionsByGroup)
    {
        GroupTicks.clear();
    }
}

void FTickManager::GatherTickFunctions(UWorld* World, ELevelTick TickType)
{
	Reset();

	if (!World)
	{
		return;
	}

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }
 
        QueueTickFunction(Actor->PrimaryActorTick);
 
        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (!Component)
            {
                continue;
            }
            QueueTickFunction(Component->PrimaryComponentTick);
        }
    }
}

void FTickManager::QueueTickFunction(FTickFunction& TickFunction)
{
    if (!TickFunction.IsTickFunctionRegistered())
    {
        return;
    }
 
    const ETickingGroup Group = TickFunction.GetTickGroup();
    if (Group < 0 || Group >= TG_MAX)
    {
        return;
    }
 
    TickFunctionsByGroup[Group].push_back(&TickFunction);
}
