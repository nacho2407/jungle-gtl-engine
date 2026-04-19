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
	Ar << bEditorOnly;
	Ar << bIsActive;
	Ar << bAutoActivate;
}

void UActorComponent::SetEditorOnly(bool bInEditorOnly)
{
	if (bEditorOnly == bInEditorOnly) return;
	bEditorOnly = bInEditorOnly;

	// 렌더 상태 재생성 — EditorOnly 변경 시 프록시 생성/파괴 판단이 달라짐
	DestroyRenderState();
	CreateRenderState();
}

void UActorComponent::SetOwner(AActor* Actor)
{
	Owner = Actor;
	PrimaryComponentTick.Target = this;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = bTickEnable;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	//OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	//OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	//OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
	OutProps.push_back({ "bTickEnable", EPropertyType::Bool, &bTickEnable });
	OutProps.push_back({ "bEditorOnly", EPropertyType::Bool, &bEditorOnly });
}

void UActorComponent::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "bTickEnable") == 0) {
		PrimaryComponentTick.SetTickEnabled(bTickEnable);
	}

	if (strcmp(PropertyName, "bEditorOnly") == 0) {
		// Property Editor가 bEditorOnly를 이미 직접 수정한 상태이므로
		// SetEditorOnly의 early-return 가드를 우회하여 렌더 상태를 직접 재생성한다.
		DestroyRenderState();
		CreateRenderState();
	}
}
