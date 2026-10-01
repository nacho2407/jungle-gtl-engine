// 엔진 영역의 세부 동작을 구현합니다.
#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

namespace
{
bool ShouldQueueActorTick(const AActor* Actor, ELevelTick TickType)
{
    if (!Actor)
    {
        return false;
    }

    switch (TickType)
    {
    case LEVELTICK_ViewportsOnly:
        return Actor->bTickInEditor;

    case LEVELTICK_All:
    case LEVELTICK_PauseTick:
        return Actor->IsActorTickEnabled() && Actor->HasActorBegunPlay();

    case LEVELTICK_TimeOnly:
    default:
        return false;
    }
}

bool ShouldQueueComponentTicks(const AActor* Actor, ELevelTick TickType)
{
    if (!Actor)
    {
        return false;
    }

    switch (TickType)
    {
    case LEVELTICK_ViewportsOnly:
        // 현재 엔진에는 Component 전용 bTickInEditor가 없으므로 기존과 동일하게 Owner Actor의 bTickInEditor를 사용
        return Actor->bTickInEditor;

    case LEVELTICK_All:
    case LEVELTICK_PauseTick:
        // 핵심 변경점: Runtime / PIE Component Tick은 Actor.bNeedsTick에 의존하지 않음
        return Actor->HasActorBegunPlay();

    case LEVELTICK_TimeOnly:
    default:
        return false;
    }
}
} // namespace

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

void FTickManager::Tick(UWorld* World, float DeltaTime, ELevelTick TickType)
{
    GatherTickFunctions(World, TickType);

    for (int GroupIndex = 0; GroupIndex < TG_MAX; ++GroupIndex)
    {
        const ETickingGroup CurrentGroup = static_cast<ETickingGroup>(GroupIndex);
        for (FTickFunction* TickFunction : TickFunctions)
        {
            if (!TickFunction || TickFunction->GetTickGroup() != CurrentGroup)
            {
                continue;
            }

            if (!TickFunction->CanTick(TickType))
            {
                continue;
            }

            if (!TickFunction->ConsumeInterval(DeltaTime))
            {
                continue;
            }

            TickFunction->ExecuteTick(DeltaTime, TickType);
        }
    }
}

void FTickManager::Reset()
{
    TickFunctions.clear();
}

void FTickManager::GatherTickFunctions(UWorld* World, ELevelTick TickType)
{
    TickFunctions.clear();

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

        if (ShouldQueueActorTick(Actor, TickType))
        {
            QueueTickFunction(Actor->PrimaryActorTick);
        }

        if (!ShouldQueueComponentTicks(Actor, TickType))
        {
            continue;
        }

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
    if (!TickFunction.bRegistered)
    {
        TickFunction.RegisterTickFunction();
    }

    TickFunctions.push_back(&TickFunction);
}

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
    if (Target)
    {
        Target->TickActor(DeltaTime, TickType, *this);
    }
}

const char* FActorTickFunction::GetDebugName() const
{
    return Target ? Target->GetClass()->GetName() : "FActorTickFunction";
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
    if (Target)
    {
        Target->TickComponent(DeltaTime, TickType, *this);
    }
}

const char* FActorComponentTickFunction::GetDebugName() const
{
    return Target ? Target->GetClass()->GetName() : "FActorComponentTickFunction";
}
