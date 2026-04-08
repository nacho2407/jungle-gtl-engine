#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"

//TODO: Actor 에디터 Only인지 파악하고 Tick을 보내야함
void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{	
	if (Target)
	{
		if (TickType != LEVELTICK_ViewportsOnly )
		{
			Target->TickActor(DeltaTime, TickType, *this);
		}
	}
}

const char* FActorTickFunction::GetDebugName() const
{
	return Target ? Target->GetTypeInfo()->name : "FActorTickFunction";
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		if (TickType != LEVELTICK_ViewportsOnly )
		{
			Target->TickComponent(DeltaTime);
		}
	}
}

const char* FActorComponentTickFunction::GetDebugName() const
{
	return Target ? Target->GetTypeInfo()->name : "FActorComponentTickFunction";
}
