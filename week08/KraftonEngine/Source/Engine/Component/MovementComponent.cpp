#include "Component/MovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UMovementComponent, UActorComponent)

void UMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	TryAutoRegisterUpdatedComponent();
}

void UMovementComponent::Tick(float DeltaTime)
{
	// 기본 이동 컴포넌트는 별도 로직 없이 틱 파이프라인만 유지합니다.
	UActorComponent::Tick(DeltaTime);
}

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Auto Register Updated", EPropertyType::Bool, &bAutoRegisterUpdatedComponent });
}

void UMovementComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	// UpdatedComponent 포인터는 BeginPlay에서 재해결되므로 직렬화 제외.
	Ar << bAutoRegisterUpdatedComponent;
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
}

void UMovementComponent::TryAutoRegisterUpdatedComponent()
{
	if (!bAutoRegisterUpdatedComponent || UpdatedComponent != nullptr)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	SetUpdatedComponent(OwnerActor->GetRootComponent());
}
