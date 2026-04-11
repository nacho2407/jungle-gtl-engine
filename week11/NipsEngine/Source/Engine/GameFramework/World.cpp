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

// 새로운 UWorld 인스턴스를 생성하고, PIE 월드를 사용하기 위해 내부 액터를 깊은 복사한다.
UWorld* UWorld::Duplicate()
{
    UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();

    if (NewWorld->PersistentLevel)
    {
        UObjectManager::Get().DestroyObject(NewWorld->PersistentLevel);
        NewWorld->PersistentLevel = nullptr;
    }

    NewWorld->CopyPropertiesFrom(this);

    // 프로퍼티 시스템에 노출되지 않은 나머지 필드를 직접 처리합니다.
    NewWorld->WorldType = this->WorldType;
    NewWorld->ActiveCamera = this->ActiveCamera;
    NewWorld->bHasBegunPlay = false;                   // 항상 미시작 상태로 시작
    NewWorld->PersistentLevel = this->PersistentLevel; // DuplicateSubObjects 에서 깊은 복사

    NewWorld->DuplicateSubObjects();

    return NewWorld;
}

/* @brief Level을 깊은 복사한 뒤, 복제된 액터들의 소속을 자기 자신으로 재설정합니다.*/
UWorld* UWorld::DuplicateSubObjects()
{
    if (PersistentLevel)
    {
        PersistentLevel = PersistentLevel->Duplicate();

        for (AActor* DuplicatedActor : PersistentLevel->GetActors())
        {
            if (DuplicatedActor)
            {
                DuplicatedActor->SetWorld(this);
            }
        }
    }

    RebuildSpatialIndex();

    return this;
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
