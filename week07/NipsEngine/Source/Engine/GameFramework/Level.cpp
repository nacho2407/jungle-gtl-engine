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

    NewLevel->Actors = this->Actors;

	NewLevel->DuplicateSubObjects();

    return NewLevel;
}

/* @brief 복제된 배열을 순회하며 액터를 깊은 복사로 교체합니다. */
ULevel* ULevel::DuplicateSubObjects()
{
    for (int32 i = 0; i < Actors.size(); ++i)
    {
        if (Actors[i])
        {
            Actors[i] = Actors[i]->Duplicate();
        }
    }
	
	return this;
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

void ULevel::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay(EndPlayReason);
		}
	}
}
