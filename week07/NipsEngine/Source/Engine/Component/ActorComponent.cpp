#include "ActorComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UActorComponent, UObject)
REGISTER_FACTORY(UActorComponent)

// 기본 상태 변수를 복사하되, Owner에 대한 정보는 복사하지 않는다.
UActorComponent* UActorComponent::Duplicate()
{
    UActorComponent* NewComp = UObjectManager::Get().CreateObject<UActorComponent>();
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
	OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
	OutProps.push_back({ "Be Serialized", EPropertyType::Bool, &bTransient });
	OutProps.push_back({ "Editor Only", EPropertyType::Bool, &bIsEditorOnly });
}

// 컴포넌트들의 프로퍼티를 GetEditableProperties()에 노출된 프로퍼티 정보를 바탕으로 알아서 잘 복사해 줍니다.
void UActorComponent::CopyPropertiesFrom(UActorComponent* Src)
{
    if (!Src) return;

    TArray<FPropertyDescriptor> SrcProps;
    Src->GetEditableProperties(SrcProps);

    TArray<FPropertyDescriptor> DstProps;
    this->GetEditableProperties(DstProps);

    // 이름 기반 매칭: 순서가 동일하므로 인덱스 직접 매칭도 가능하지만 안전성을 위해 Name으로 찾습니다.
    for (const FPropertyDescriptor& SrcProp : SrcProps)
    {
        // 대응하는 Dst 프로퍼티 탐색
        FPropertyDescriptor* DstProp = nullptr;
        for (FPropertyDescriptor& D : DstProps)
        {
            if (strcmp(D.Name, SrcProp.Name) == 0)
            {
                DstProp = &D;
                break;
            }
        }
        if (!DstProp) continue;

        switch (SrcProp.Type)
        {
        case EPropertyType::Bool:
        case EPropertyType::Int:
        case EPropertyType::Float:
        case EPropertyType::Vec3:
        case EPropertyType::Vec4:
        {
            size_t Size = GetPropertySize(SrcProp.Type);
            if (Size > 0)
                memcpy(DstProp->ValuePtr, SrcProp.ValuePtr, Size);
            break;
        }
        case EPropertyType::String:
            *static_cast<FString*>(DstProp->ValuePtr) =
                *static_cast<const FString*>(SrcProp.ValuePtr);
            break;

        case EPropertyType::Name:
            *static_cast<FName*>(DstProp->ValuePtr) =
                *static_cast<const FName*>(SrcProp.ValuePtr);
			// FName은 Resource Key이기 때문에 캐시 갱신 후에 호출해야 합니다.
			this->PostEditProperty(SrcProp.Name);
            break;

		// Duplicate에서 포인터 복사는 건너뜁니다. 이 부분은 Actor의 Duplicate()가 알아서 잘 복사해줘야 합니다.
		// 이유: 컴포넌트 입장에선 자기 부모 컴포넌트나 자식 컴포넌트들이 어느 주소로 복사될지 알 수가 없습니다.
        case EPropertyType::SceneComponentRef:
            break;
        }
    }

	/** 위 함수는 성능상 프로퍼티 개수 N에 대해 O(N^2)이므로 개선의 여지가 있습니다. 
	 * 제미나이에 따르면 추후 FPropertyDescriptor에 해시 및 인덱스를 추가하면 된다고 합니다.
	 * 다만 O(N·logN) 개선이 유의미할 정도로 프로퍼티 수가 많은 컴포넌트가 아직은 없는 것 같습니다.
	 **/
}