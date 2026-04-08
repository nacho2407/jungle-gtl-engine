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

    if (NewWorld->PersistentLevel)
    {
        delete NewWorld->PersistentLevel;
    }

    if (this->PersistentLevel)
    {
        NewWorld->PersistentLevel = this->PersistentLevel->Duplicate();

		// 복제된 액터들은 월드 소속을 새 월드로 바꿔주어야 한다.
        for (AActor* DuplicatedActor : NewWorld->PersistentLevel->GetActors())
        {
            if (DuplicatedActor)
            {
                DuplicatedActor->SetWorld(NewWorld);
            }
        }
    }

    NewWorld->SetActiveCamera(this->ActiveCamera);
    NewWorld->bHasBegunPlay = false;

    return NewWorld;
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;
	PersistentLevel->BeginPlay();
}

void UWorld::Tick(float DeltaTime)
{
	PersistentLevel->Tick(DeltaTime);
}

void UWorld::EndPlay()
{
	if(bHasBegunPlay)
	{
		bHasBegunPlay = false;
		PersistentLevel->EndPlay();
	}
}
