#include "GameFramework/World.h"

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

void UWorld::Tick(float DeltaTime)
{
    if (!PersistentLevel)
        return;

    for (AActor* Actor : PersistentLevel->GetActors())
    {
        if (Actor && Actor->IsActive())
        {
            // 에디터 월드일 경우, 에디터 틱이 허용된 액터만 Tick을 수행합니다.
            if (WorldType == EWorldType::Editor && !Actor->ShouldTickInEditor())
            {
                continue;
            }
            Actor->Tick(DeltaTime);
        }
    }

    SyncSpatialIndex();
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
