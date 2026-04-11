#include "ActorComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UActorComponent, UObject)
REGISTER_FACTORY(UActorComponent)

// 기본 상태 변수를 복사하되, Owner에 대한 정보는 복사하지 않는다.
UActorComponent* UActorComponent::Duplicate()
{
    UActorComponent* NewComp =
        UObjectManager::Get().CreateObject<UActorComponent>();
    NewComp->CopyPropertiesFrom(this);
    return NewComp;
}

void UActorComponent::BeginPlay()
{
    if (bAutoActivate)
    {
        Activate();
    }
}

void UActorComponent::Activate()
{
    bCanEverTick = true;
}

void UActorComponent::Deactivate()
{
    bCanEverTick = false;
}

void UActorComponent::ExecuteTick(float DeltaTime)
{
    if (bCanEverTick == false || bIsActive == false)
    {
        return;
    }

    TickComponent(DeltaTime);
}

void UActorComponent::SetActive(bool bNewActive)
{
    if (bNewActive == bIsActive)
    {
        return;
    }

    bIsActive = bNewActive;

    if (bIsActive)
    {
        Activate();
    }
    else
    {
        Deactivate();
    }
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    // OutProps.push_back({"Active", EPropertyType::Bool, &bIsActive});
    // OutProps.push_back({"Auto Activate", EPropertyType::Bool, &bAutoActivate});
    OutProps.push_back({"Enable Tick", EPropertyType::Bool, &bCanEverTick});
    OutProps.push_back({"Be Serialized", EPropertyType::Bool, &bTransient});
    OutProps.push_back({"Editor Only", EPropertyType::Bool, &bIsEditorOnly});
}