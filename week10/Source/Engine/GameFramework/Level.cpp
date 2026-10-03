#include "Level.h"

#include <algorithm>

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
    for (AActor* Actor : PendingAddActors)
    {
        if (Actor && !ContainsActor(Actors, Actor))
        {
            UObjectManager::Get().DestroyObject(Actor);
        }
    }

    Actors.clear();
    PendingAddActors.clear();
    PendingRemoveActors.clear();
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

bool ULevel::ContainsActor(const TArray<AActor*>& ActorList, AActor* Actor) const
{
    return Actor && std::find(ActorList.begin(), ActorList.end(), Actor) != ActorList.end();
}

void ULevel::AddActor(AActor* Actor)
{
    if (!Actor || ContainsActor(Actors, Actor) || ContainsActor(PendingAddActors, Actor))
    {
        return;
    }

    if (bIteratingActors)
    {
        PendingAddActors.push_back(Actor);
        return;
    }

    Actors.push_back(Actor);
}

void ULevel::RemoveActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    PendingAddActors.erase(
        std::remove(PendingAddActors.begin(), PendingAddActors.end(), Actor),
        PendingAddActors.end());

    if (bIteratingActors)
    {
        for (AActor*& ExistingActor : Actors)
        {
            if (ExistingActor == Actor)
            {
                ExistingActor = nullptr;
                break;
            }
        }

        if (!ContainsActor(PendingRemoveActors, Actor))
        {
            PendingRemoveActors.push_back(Actor);
        }
        return;
    }

    Actors.erase(std::remove(Actors.begin(), Actors.end(), Actor), Actors.end());
}

void ULevel::FlushPendingActorMutations()
{
    Actors.erase(
        std::remove_if(
            Actors.begin(),
            Actors.end(),
            [this](AActor* Actor)
            {
                return !Actor || ContainsActor(PendingRemoveActors, Actor);
            }),
        Actors.end());

    for (AActor* Actor : PendingAddActors)
    {
        if (Actor && !ContainsActor(PendingRemoveActors, Actor) && !ContainsActor(Actors, Actor))
        {
            Actors.push_back(Actor);
        }
    }

    PendingAddActors.clear();
    PendingRemoveActors.clear();
}

void ULevel::BeginPlay()
{
    bIteratingActors = true;
    const int32 InitialActorCount = static_cast<int32>(Actors.size());
	for (int32 Index = 0; Index < InitialActorCount && Index < static_cast<int32>(Actors.size()); ++Index)
	{
        AActor* Actor = Actors[Index];
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
    bIteratingActors = false;
    FlushPendingActorMutations();
}

void ULevel::TickEditor(float DeltaTime)
{
    bIteratingActors = true;
    const int32 InitialActorCount = static_cast<int32>(Actors.size());
    for (int32 Index = 0; Index < InitialActorCount && Index < static_cast<int32>(Actors.size()); ++Index)
    {
        AActor* Actor = Actors[Index];
        if (Actor && Actor->IsActive() && Actor->ShouldTickInEditor())
        {
            Actor->Tick(DeltaTime);
        }
    }
    bIteratingActors = false;
    FlushPendingActorMutations();
}

void ULevel::TickGame(float DeltaTime)
{
    bIteratingActors = true;
    const int32 InitialActorCount = static_cast<int32>(Actors.size());
    for (int32 Index = 0; Index < InitialActorCount && Index < static_cast<int32>(Actors.size()); ++Index)
    {
        AActor* Actor = Actors[Index];
        if (Actor && Actor->IsActive())
        {
            Actor->Tick(DeltaTime);
            if (Actor->GetActorLocation().Z < -5)
                Actor->MarkPendingKill();
        }
    }
    bIteratingActors = false;
    FlushPendingActorMutations();
}

void ULevel::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    bIteratingActors = true;
    const int32 InitialActorCount = static_cast<int32>(Actors.size());
	for (int32 Index = 0; Index < InitialActorCount && Index < static_cast<int32>(Actors.size()); ++Index)
	{
        AActor* Actor = Actors[Index];
		if (Actor)
		{
			Actor->EndPlay(EndPlayReason);
		}
	}
    bIteratingActors = false;
    FlushPendingActorMutations();
}
