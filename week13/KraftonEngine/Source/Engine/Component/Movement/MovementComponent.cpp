#include "Component/Movement/MovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cstring>
#include <Object/GarbageCollection.h>

// Base movement logic only; concrete movement types should be added instead.
HIDE_FROM_COMPONENT_LIST(UMovementComponent)

namespace
{
	void GatherSceneComponentsRecursive(USceneComponent* Component, TArray<USceneComponent*>& OutComponents)
	{
		if (!IsValid(Component))
		{
			return;
		}

		OutComponents.push_back(Component);
		for (USceneComponent* Child : Component->GetChildren())
		{
			GatherSceneComponentsRecursive(Child, OutComponents);
		}
	}
}

void UMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	ResolveUpdatedComponent();
}

void UMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	// 기본 이동 컴포넌트는 별도 로직 없이 틱 파이프라인만 유지합니다.
	UActorComponent::TickComponent(DeltaTime,TickType, ThisTickFunction);
}

void UMovementComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "bAutoRegisterUpdatedComponent") == 0 || std::strcmp(PropertyName, "Auto Register Updated") == 0)
	{
		if (bAutoRegisterUpdatedComponent && !UpdatedComponent)
		{
			TryAutoRegisterUpdatedComponent();
		}
		return;
	}

	if (std::strcmp(PropertyName, "UpdatedComponent") == 0 || std::strcmp(PropertyName, "Updated Component") == 0)
	{
		ResolveUpdatedComponent();
	}
}


void UMovementComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UActorComponent::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(UpdatedComponent, "UMovementComponent.UpdatedComponent");
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (!IsValid(NewUpdatedComponent))
	{
		UpdatedComponent = nullptr;
		bAutoRegisterUpdatedComponent = true;
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || NewUpdatedComponent->GetOwner() != OwnerActor)
	{
		UpdatedComponent = nullptr;
		bAutoRegisterUpdatedComponent = true;
		return;
	}

	UpdatedComponent = NewUpdatedComponent;
	bAutoRegisterUpdatedComponent = false;
}

USceneComponent* UMovementComponent::GetUpdatedComponent() const
{
	return IsValid(UpdatedComponent.GetRaw()) ? UpdatedComponent.GetRaw() : nullptr;
}

void UMovementComponent::ClearUpdatedComponentIfMatches(const USceneComponent* RemovedComponent)
{
	if (!RemovedComponent || UpdatedComponent.GetRaw() != RemovedComponent)
	{
		return;
	}

	UpdatedComponent = nullptr;
	bAutoRegisterUpdatedComponent = true;
	TryAutoRegisterUpdatedComponent();
}

void UMovementComponent::TryAutoRegisterUpdatedComponent()
{
	if (!bAutoRegisterUpdatedComponent)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	USceneComponent* RootComponent = OwnerActor->GetRootComponent();
	UpdatedComponent = IsValid(RootComponent) ? RootComponent : nullptr;
}

TArray<USceneComponent*> UMovementComponent::GetOwnerSceneComponents() const
{
	TArray<USceneComponent*> Components;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return Components;
	}

	GatherSceneComponentsRecursive(OwnerActor->GetRootComponent(), Components);
	return Components;
}

FString UMovementComponent::GetUpdatedComponentDisplayName() const
{
	USceneComponent* CurrentUpdatedComponent = GetUpdatedComponent();
	if (!CurrentUpdatedComponent)
	{
		return bAutoRegisterUpdatedComponent ? FString("Auto (Root)") : FString("None");
	}

	FString DisplayName = CurrentUpdatedComponent->GetFName().ToString();
	if (DisplayName.empty())
	{
		DisplayName = CurrentUpdatedComponent->GetClass()->GetName();
	}
	return DisplayName;
}

bool UMovementComponent::ResolveUpdatedComponent()
{
	if (USceneComponent* CurrentUpdatedComponent = GetUpdatedComponent())
	{
		for (USceneComponent* Candidate : GetOwnerSceneComponents())
		{
			if (Candidate == CurrentUpdatedComponent)
			{
				bAutoRegisterUpdatedComponent = false;
				return true;
			}
		}
	}

	UpdatedComponent = nullptr;
	if (bAutoRegisterUpdatedComponent)
	{
		TryAutoRegisterUpdatedComponent();
	}
	return UpdatedComponent != nullptr;
}
