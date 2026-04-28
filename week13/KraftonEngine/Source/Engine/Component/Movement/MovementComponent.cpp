#include "Component/Movement/MovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

// Base movement logic only; concrete movement types should be added instead.
IMPLEMENT_CLASS(UMovementComponent, UActorComponent)
HIDE_FROM_COMPONENT_LIST(UMovementComponent)

namespace
{
	constexpr const char* RootComponentPathToken = "Root";

	void GatherSceneComponentsRecursive(USceneComponent* Component, TArray<USceneComponent*>& OutComponents)
	{
		if (!Component)
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

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Auto Register Updated", EPropertyType::Bool, &bAutoRegisterUpdatedComponent });
	OutProps.push_back({ "Updated Component", EPropertyType::SceneComponentRef, &UpdatedComponentPath });
}

void UMovementComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << bAutoRegisterUpdatedComponent;
	Ar << UpdatedComponentPath;
	// UpdatedComponent 포인터는 BeginPlay에서 재해결되므로 직렬화 제외.
}

void UMovementComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Auto Register Updated") == 0)
	{
		if (bAutoRegisterUpdatedComponent && UpdatedComponentPath.empty())
		{
			TryAutoRegisterUpdatedComponent();
		}
		return;
	}

	if (std::strcmp(PropertyName, "Updated Component") == 0)
	{
		ResolveUpdatedComponent();
	}
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
	UpdatedComponentPath = BuildUpdatedComponentPath(NewUpdatedComponent);
	if (UpdatedComponent)
	{
		bAutoRegisterUpdatedComponent = false;
	}
}

USceneComponent* UMovementComponent::GetUpdatedComponent() const
{
	return UpdatedComponent;
}

void UMovementComponent::ClearUpdatedComponentIfMatches(const USceneComponent* RemovedComponent)
{
	if (!RemovedComponent || UpdatedComponent != RemovedComponent)
	{
		return;
	}

	UpdatedComponent = nullptr;
	UpdatedComponentPath.clear();
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

	UpdatedComponent = OwnerActor->GetRootComponent();
	UpdatedComponentPath.clear();
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

	const FString ComponentPath = BuildUpdatedComponentPath(CurrentUpdatedComponent);
	if (!ComponentPath.empty())
	{
		DisplayName += " (" + ComponentPath + ")";
	}
	return DisplayName;
}

bool UMovementComponent::ResolveUpdatedComponent()
{
	if (!UpdatedComponentPath.empty())
	{
		if (USceneComponent* ResolvedComponent = FindUpdatedComponentByPath(UpdatedComponentPath))
		{
			UpdatedComponent = ResolvedComponent;
			bAutoRegisterUpdatedComponent = false;
			return true;
		}
	}

	UpdatedComponent = nullptr;
	bAutoRegisterUpdatedComponent = true;
	TryAutoRegisterUpdatedComponent();
	return UpdatedComponent != nullptr;
}

FString UMovementComponent::BuildUpdatedComponentPath(const USceneComponent* TargetComponent) const
{
	if (!TargetComponent)
	{
		return FString();
	}

	AActor* OwnerActor = GetOwner();
	USceneComponent* RootComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	if (!RootComponent)
	{
		return FString();
	}

	if (TargetComponent == RootComponent)
	{
		return RootComponentPathToken;
	}

	TArray<int32> PathIndices;
	const USceneComponent* Current = TargetComponent;
	while (Current && Current != RootComponent)
	{
		const USceneComponent* Parent = Current->GetParent();
		if (!Parent)
		{
			return FString();
		}

		const TArray<USceneComponent*>& Siblings = Parent->GetChildren();
		int32 ChildIndex = -1;
		for (int32 Index = 0; Index < static_cast<int32>(Siblings.size()); ++Index)
		{
			if (Siblings[Index] == Current)
			{
				ChildIndex = Index;
				break;
			}
		}
		if (ChildIndex < 0)
		{
			return FString();
		}

		PathIndices.push_back(ChildIndex);
		Current = Parent;
	}

	if (Current != RootComponent)
	{
		return FString();
	}

	FString Path = RootComponentPathToken;
	for (auto It = PathIndices.rbegin(); It != PathIndices.rend(); ++It)
	{
		Path += "/";
		Path += std::to_string(*It);
	}
	return Path;
}

USceneComponent* UMovementComponent::FindUpdatedComponentByPath(const FString& InPath) const
{
	if (InPath.empty())
	{
		return nullptr;
	}

	AActor* OwnerActor = GetOwner();
	USceneComponent* Current = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	if (!Current)
	{
		return nullptr;
	}

	if (InPath == RootComponentPathToken)
	{
		return Current;
	}

	std::stringstream Stream(InPath);
	FString Segment;
	bool bExpectRoot = true;
	while (std::getline(Stream, Segment, '/'))
	{
		if (Segment.empty())
		{
			continue;
		}

		if (bExpectRoot)
		{
			bExpectRoot = false;
			if (Segment != RootComponentPathToken)
			{
				return nullptr;
			}
			continue;
		}

		char* ParseEnd = nullptr;
		const long ParsedIndex = std::strtol(Segment.c_str(), &ParseEnd, 10);
		if (ParseEnd == Segment.c_str() || *ParseEnd != '\0')
		{
			return nullptr;
		}

		const int32 ChildIndex = static_cast<int32>(ParsedIndex);
		const TArray<USceneComponent*>& Children = Current->GetChildren();
		if (ChildIndex < 0 || ChildIndex >= static_cast<int32>(Children.size()))
		{
			return nullptr;
		}

		Current = Children[ChildIndex];
		if (!Current)
		{
			return nullptr;
		}
	}

	return Current;
}
