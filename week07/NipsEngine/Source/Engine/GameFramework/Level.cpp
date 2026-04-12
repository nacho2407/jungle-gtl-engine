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

/* @brief 액터 배열을 얕은 복사한 뒤 각 액터를 깊은 복사로 교체합니다. */
void ULevel::PostDuplicate(UObject* Original)
{
    const ULevel* OrigLevel = Cast<ULevel>(Original);
    Actors = OrigLevel->Actors; // 얕은 복사
    for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
    {
        if (Actors[i])
        {
            Actors[i] = Cast<AActor>(Actors[i]->Duplicate()); // 깊은 복사로 교체
        }
    }
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
		if (Actor && Actor->IsActive())
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
