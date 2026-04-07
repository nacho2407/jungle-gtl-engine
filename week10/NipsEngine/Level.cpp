#include "Level.h"

DEFINE_CLASS(ULevel, UObject)
REGISTER_FACTORY(ULevel)

void ULevel::BeginPlay()
{
		for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
		}
}

void ULevel::Tick(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::EndPlay()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
		}
	}
}
