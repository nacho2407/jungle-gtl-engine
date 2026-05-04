#include "GameFramework/World.h"
#include "Engine/Collision/Collision.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/PlayerController.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

// FName, UUID 발급, 메모리 추적 등을 위해 UObjectManager를 통해 생성, 삭제한다.
UWorld::UWorld()
{
    PersistentLevel = UObjectManager::Get().CreateObject<ULevel>();
}

// 소멸 역시 UObjectManager를 통해 처리한다.
UWorld::~UWorld()
{
    SpatialIndex.Clear();
    UObjectManager::Get().DestroyObject(PersistentLevel);
}

/* @brief 비노출 필드를 복사하고, Level을 깊은 복사한 뒤, 복제된 액터들의 소속을 자기 자신으로 재설정합니다. */
void UWorld::PostDuplicate(UObject* Original)
{
    // UWorld 생성자가 기본 PersistentLevel을 생성하므로,
    // 원본의 레벨로 교체하기 전에 먼저 해제합니다.
    if (PersistentLevel)
    {
        UObjectManager::Get().DestroyObject(PersistentLevel);
        PersistentLevel = nullptr;
    }

    const UWorld* OrigWorld = Cast<UWorld>(Original);

    // 프로퍼티 시스템에 노출되지 않은 필드를 직접 복사합니다.
    WorldType      = OrigWorld->WorldType;
    ActiveCamera   = OrigWorld->ActiveCamera;
    bHasBegunPlay  = false; // 항상 미시작 상태로 시작

    // PersistentLevel 을 깊은 복사한 뒤, 복제된 액터들의 소속을 새 월드로 재설정합니다.
    if (OrigWorld->PersistentLevel)
    {
        PersistentLevel = Cast<ULevel>(OrigWorld->PersistentLevel->Duplicate());
        for (AActor* DuplicatedActor : PersistentLevel->GetActors())
        {
            if (DuplicatedActor)
            {
                DuplicatedActor->SetWorld(this);
            }
        }
    }

    RebuildSpatialIndex();
}

void UWorld::BeginPlay()
{
    bHasBegunPlay = true;
    PersistentLevel->BeginPlay();
    RebuildSpatialIndex();
}

AActor* UWorld::SpawnActorByTypeName(const FString& TypeName)
{
    UObject* Object = FObjectFactory::Get().Create(TypeName);
    AActor* Actor = Cast<AActor>(Object);
    if (!Actor)
    {
        if (Object)
        {
            UObjectManager::Get().DestroyObject(Object);
        }
        UE_LOG_ERROR("[World] Failed to spawn actor by type name: %s", TypeName.c_str());
        return nullptr;
    }

    Actor->SetWorld(this);
    Actor->InitDefaultComponents();
    if (bHasBegunPlay)
    {
        Actor->BeginPlay();
    }
    PersistentLevel->AddActor(Actor);
    SpatialIndex.FlushDirtyBounds();
    return Actor;
}

void UWorld::Tick(float DeltaTime)
{
    if (!PersistentLevel)
        return;

	if (WorldType == EWorldType::Editor)
		PersistentLevel->TickEditor(DeltaTime);
	else
		PersistentLevel->TickGame(DeltaTime);

    SyncSpatialIndex();
    UpdateOverlaps();
    CheckPendingKill();
}

void UWorld::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (bHasBegunPlay)
    {
        bHasBegunPlay = false;
        PersistentLevel->EndPlay(EndPlayReason);
    }
}

void UWorld::RebuildSpatialIndex()
{
    SpatialIndex.Rebuild(this);
}

void UWorld::SyncSpatialIndex()
{
    SpatialIndex.FlushDirtyBounds();
}

int32 UWorld::AddActorDestroyedListener(FActorDestroyedListener Listener)
{
    const int32 Id = NextActorDestroyedListenerId++;
    ActorDestroyedListeners[Id] = std::move(Listener);
    return Id;
}

void UWorld::RemoveActorDestroyedListener(int32 ListenerId)
{
    ActorDestroyedListeners.erase(ListenerId);
}

void UWorld::NotifyActorDestroyed(AActor* Actor)
{
    if (Actor)
    {
        TArray<AActor*> Actors = PersistentLevel ? PersistentLevel->GetActors() : TArray<AActor*>();
        for (AActor* WorldActor : Actors)
        {
            APlayerController* Controller = Cast<APlayerController>(WorldActor);
            if (Controller && Controller != Actor)
            {
                Controller->NotifyObservedActorDestroyed(Actor);
            }
        }
    }

    for (auto& [Id, Listener] : ActorDestroyedListeners)
    {
        if (Listener)
        {
            Listener(Actor);
        }
    }
}

void UWorld::UpdateOverlaps()
{
	if (PersistentLevel)
	{
        TArray<UPrimitiveComponent*> CollisionCandidates;
		for (AActor* Actor : PersistentLevel->GetActors())
		{
			for (UActorComponent* Comp : Actor->GetComponents())
			{
                UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp);
				if (PrimComp)
				{
                    PrimComp->SetPrevOverlaps(PrimComp->GetOverlapInfos());
                    PrimComp->ClearOverlaps();
                    if (PrimComp->ShouldGenerateOverlapEvents())
                        CollisionCandidates.push_back(PrimComp);
				}
			}
		}
        
		for (int i = 0; i < CollisionCandidates.size(); ++i)
        {
            UPrimitiveComponent* A = CollisionCandidates[i];
            for (int j = i+1; j < CollisionCandidates.size(); ++j)
			{
                UPrimitiveComponent* B = CollisionCandidates[j];
				// 같은 액터 타입 끼리는 Overlap 하지 않는다 => 게임잼 내 가정
                if (A != B && A->GetOwner()->GetTypeInfo() != B->GetOwner()->GetTypeInfo())
				{
					// Normal 의 경우 A -> B 방향
					FCollisionResult CollisionResult = FCollision::CheckOverlap(A, B);

					if (CollisionResult.bHit)
                    {
                        A->AddOverlap(B, CollisionResult);
                        // B -> A 방향으로 바꿔주기
                        CollisionResult.HitNormal *= -1;
                        B->AddOverlap(A, CollisionResult);
					}
				}
			}
            A->ResolveOverlaps();
		}
	}
}

void UWorld::CheckPendingKill()
{
	if (PersistentLevel)
	{
        TArray<AActor*> PendingKillActors;
		for (AActor* Actor : PersistentLevel->GetActors())
		{
			if (Actor->IsPendingKill())
			{
                PendingKillActors.push_back(Actor);
			}
		}

		for (AActor* Actor : PendingKillActors)
            DestroyActor(Actor);
	}
}
