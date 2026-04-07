#include "GameFramework/World.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;
	PersistentLevel->BeginPlay();
}

void UWorld::Tick(float DeltaTime)
{
	PersistentLevel->Tick(DeltaTime);
}

void UWorld::EndPlay()
{
	if(bHasBegunPlay)
	{
		bHasBegunPlay = false;
		PersistentLevel->EndPlay();
	}
}
