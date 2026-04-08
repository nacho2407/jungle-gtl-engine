#include "Level.h"

DEFINE_CLASS(ULevel, UObject)
REGISTER_FACTORY(ULevel)

// 소멸될 때 가지고 있던 모든 액터들을 메모리에서 완전히 해제한다.
ULevel::~ULevel()
{ 
    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            UObjectManager::Get().DestroyObject(Actor);
        }
    }
    
    Actors.clear();
}

ULevel* ULevel::Duplicate()
{
    ULevel* NewLevel = UObjectManager::Get().CreateObject<ULevel>();

    for (AActor* OriginalActor : this->Actors)
    {
        if (OriginalActor)
        {
            AActor* DuplicatedActor = OriginalActor->Duplicate();
            
            if (DuplicatedActor != nullptr)
            {
                NewLevel->AddActor(DuplicatedActor);
            }
        }
    }

    return NewLevel;
}

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
