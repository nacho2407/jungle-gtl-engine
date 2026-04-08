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

// 새로운 UWorld 인스턴스를 생성하고, PIE 월드를 사용하기 위해 내부 액터를 깊은 복사한다.
UWorld* UWorld::Duplicate()
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	NewWorld->SetActiveCamera(this->ActiveCamera);
	NewWorld->bHasBegunPlay = false;

	for (AActor* OriginalActor : this->Actors)
	{
		if (OriginalActor)
		{
			AActor* DuplicatedActor = OriginalActor->Duplicate();
			DuplicatedActor->SetWorld(NewWorld);
			NewWorld->AddActor(DuplicatedActor);
		}
	}

	return NewWorld;
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
