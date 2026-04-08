#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

namespace
{
	bool ShouldDispatchActorTick(const AActor* Actor, ELevelTick TickType)
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
		case LEVELTICK_TimeOnly:
		case LEVELTICK_PauseTick:
			return Actor->bNeedsTick && Actor->HasActorBegunPlay();

		default:
			return false;
		}
	}
}

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickActor(DeltaTime, TickType, *this);
	}
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
		if (!ShouldDispatchActorTick(Actor, TickType))
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
	if (!TickFunction.bRegistered)
	{
		TickFunction.RegisterTickFunction();
	}

	TickFunctions.push_back(&TickFunction);
}

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

const char* FActorTickFunction::GetDebugName() const
{
	return Target ? Target->GetTypeInfo()->name : "FActorTickFunction";
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
	return Target ? Target->GetTypeInfo()->name : "FActorComponentTickFunction";
}
