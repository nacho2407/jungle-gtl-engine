#include "GameFramework/World.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

UWorld::~UWorld()
{
	if (!Actors.empty())
	{
		EndPlay();
	}
}

void UWorld::InitWorld()
{

}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void UWorld::Tick(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
}
