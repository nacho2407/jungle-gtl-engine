#include "GameFramework/Level.h"
#include "Object/Reflection/ObjectFactory.h"
#include <GameFramework/World.h>

#include "Object/GarbageCollection.h"
#include <algorithm>

ULevel::ULevel(UWorld* OwingWorld)
	: OwingWorld(OwingWorld)
{
	Actors.clear();
}

ULevel::ULevel(const TArray<AActor*>& InActors, UWorld* World)
	: OwingWorld(World)
{
	Actors.reserve(InActors.size());
	for (AActor* Actor : InActors)
	{
		Actors.push_back(Actor);
	}
}

ULevel::~ULevel()
{
	Clear();
	OwingWorld.Reset();
}

TArray<AActor*> ULevel::GetActors() const
{
	TArray<AActor*> Result;
	Result.reserve(Actors.size());
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Result.push_back(Actor);
		}
	}
	return Result;
}

void ULevel::AddActor(AActor* Actor)
{
	if (!IsValid(Actor)) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It != Actors.end())
	{
		return;
	}
	
	Actor->SetOuter(this);
	Actors.push_back(Actor);
}

void ULevel::RemoveActor(AActor* Actor)
{
	if (!IsAliveObject(Actor)) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It == Actors.end())
	{
		return;
	}

	Actors.erase(It);
}

void ULevel::Clear()
{
	for (AActor* Actor : Actors)
	{
		if (IsAliveObject(Actor))
		{
			Actor->SetOuter(nullptr);
		}
	}

	Actors.clear();
}

void ULevel::Tick(float DeltaTime) {
	for (AActor* Actor : Actors)
	{
        if (IsValid(Actor))
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::AddReferencedObjects(FReferenceCollector& Collector)
{
    UObject::AddReferencedObjects(Collector);
}

void ULevel::BeginDestroy()
{
    if (HasAnyFlags(RF_BeginDestroy))
    {
        return;
    }

    UObject::BeginDestroy();

    EndPlay();

    for (AActor* Actor : Actors)
    {
        if (IsAliveObject(Actor))
        {
            Actor->MarkPendingKill();
            Actor->BeginDestroy();
        }
    }

    Actors.clear();
    OwingWorld.Reset();
}

void ULevel::BeginPlay()
{
	const size_t InitialCount = Actors.size();
	for (size_t i = 0; i < InitialCount; ++i)
	{
		AActor* Actor = Actors[i];
		if (IsValid(Actor) && !Actor->HasActorBegunPlay())
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::EndPlay()
{
	for (AActor* Actor : Actors)
	{
        if (IsValid(Actor))
		{
			Actor->EndPlay();
		}
	}

	for (AActor* Actor : Actors)
	{
        if (IsAliveObject(Actor))
		{
			UObjectManager::Get().DestroyObject(Actor);
		}
	}
	Actors.clear();
}