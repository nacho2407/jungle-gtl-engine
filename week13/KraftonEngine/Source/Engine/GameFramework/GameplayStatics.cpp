#include "GameFramework/GameplayStatics.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/FName.h"

AActor* FGameplayStatics::FindFirstActorByTag(const UWorld* World, const FName& Tag)
{
	if (!World) return nullptr;
	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->HasTag(Tag))
		{
			return Actor;
		}
	}
	return nullptr;
}

TArray<AActor*> FGameplayStatics::FindActorsByTag(const UWorld* World, const FName& Tag)
{
	TArray<AActor*> Result;
	if (!World) return Result;
	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->HasTag(Tag))
		{
			Result.push_back(Actor);
		}
	}
	return Result;
}
