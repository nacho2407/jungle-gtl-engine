#include "GameFramework/Level.h"
#include "Object/ObjectFactory.h"
#include <GameFramework/World.h>

IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel(UWorld* OwingWorld)
	: OwingWorld(OwingWorld)
{
	Actors.clear();
}

ULevel::ULevel(const TArray<AActor*>& Actors, UWorld* World)
	: Actors(Actors)
{
	OwingWorld = World;
}

ULevel::~ULevel()
{
	Clear();
	OwingWorld = nullptr;
}

void ULevel::AddActor(AActor* Actor)
{
	if (!Actor) return;

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
	if (!Actor) return;

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
		if (Actor)
		{
			Actor->SetOuter(nullptr);
		}
	}

	Actors.clear();
}

void ULevel::Tick(float DeltaTime) {
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::BeginPlay() {
	for (AActor* Actor : Actors) {
		if (Actor) {
			Actor->BeginPlay();
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

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			UObjectManager::Get().DestroyObject(Actor);
		}
	}
	Actors.clear();
}