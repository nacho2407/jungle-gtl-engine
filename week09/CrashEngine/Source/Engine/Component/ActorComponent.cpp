// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "ActorComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"

IMPLEMENT_CLASS(UActorComponent, UObject)

void UActorComponent::BeginPlay()
{
    if (bAutoActivate)
    {
        Activate();
    }
}

void UActorComponent::Activate()
{
    bIsActive = true;
    PrimaryComponentTick.SetTickEnabled(bTickEnable);
}

void UActorComponent::Deactivate()
{
    bIsActive = false;
    PrimaryComponentTick.SetTickEnabled(false);
}


UWorld* UActorComponent::GetWorld() const
{
    return Owner ? Owner->GetWorld() : nullptr;
}

void UActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
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

void UActorComponent::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << bTickEnable;

	if (Ar.IsLoading())
    {
        PrimaryComponentTick.SetTickEnabled(bTickEnable);
    }
}

void UActorComponent::SetOwner(AActor* Actor)
{
    Owner = Actor;
    PrimaryComponentTick.Target = this;
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEnabled = bTickEnable;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    OnOwnerChanged();
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    // OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
    // OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
    // OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
    OutProps.push_back({ "bTickEnable", EPropertyType::Bool, &bTickEnable });
}

void UActorComponent::PostEditProperty(const char* PropertyName)
{
    if (strcmp(PropertyName, "bTickEnable") == 0)
    {
        SetComponentTickEnabled(bTickEnable);
    }
}
