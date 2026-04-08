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
    UObjectManager::Get().DestroyObject(PersistentLevel); 
}

// 새로운 UWorld 인스턴스를 생성하고, PIE 월드를 사용하기 위해 내부 액터를 깊은 복사한다.
UWorld* UWorld::Duplicate()
{
    UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();

    NewWorld->WorldType = this->WorldType;
    NewWorld->SetActiveCamera(this->ActiveCamera);
    NewWorld->bHasBegunPlay = false;
    NewWorld->PersistentLevel = this->PersistentLevel;

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

	return this;
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;
	PersistentLevel->BeginPlay();
}

void UWorld::Tick(float DeltaTime)
{
	if (!PersistentLevel) return;

    for (AActor* Actor : PersistentLevel->GetActors())
    {
        if (Actor && Actor->IsActorTickEnabled())
        {
            // 에디터 월드일 경우, 에디터 틱이 허용된 액터만 Tick을 수행합니다.
            if (WorldType == EWorldType::Editor && !Actor->ShouldTickInEditor())
            {
                continue;
            }
            Actor->Tick(DeltaTime);
        }
    }
}

void UWorld::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if(bHasBegunPlay)
	{
		bHasBegunPlay = false;
		PersistentLevel->EndPlay(EEndPlayReason::Type::EndPlayInEditor);
	}
}
