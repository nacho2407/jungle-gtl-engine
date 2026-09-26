#include "Actor.h"
#include "Object/ObjectFactory.h"
#include "Component/UUIDBillboardComponent.h"
#include "Object/Class.h"
#include "Renderer/Material.h"
#include "Component/TextComponent.h"
#include "Component/SceneComponent.h"
#include "Debug/EngineLog.h"
#include "Serializer/Archive.h"
#include "Level/Level.h"
IMPLEMENT_RTTI(AActor, UObject)

namespace {
	FVector GZeroVector{};

	bool IsComponentOwnedByActor(const AActor* Actor, const UActorComponent* Component)
	{
		if (!Actor || !Component)
		{
			return false;
		}

		for (UActorComponent* OwnedComponent : Actor->GetComponents())
		{
			if (OwnedComponent == Component)
			{
				return true;
			}
		}

		return false;
	}

	bool HasNonInstanceSceneDescendant(const USceneComponent* Component)
	{
		if (!Component)
		{
			return false;
		}

		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			if (!Child)
			{
				continue;
			}

			if (!Child->IsInstanceComponent() || HasNonInstanceSceneDescendant(Child))
			{
				return true;
			}
		}

		return false;
	}

	void GatherSceneDeletionSubtree(USceneComponent* Component, TArray<UActorComponent*>& OutComponents)
	{
		if (!Component)
		{
			return;
		}

		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			GatherSceneDeletionSubtree(Child, OutComponents);
		}

		OutComponents.push_back(Component);
	}

	bool GatherSerializableOwnedComponents(const AActor* Actor, TArray<UActorComponent*>& OutComponents)
	{
		if (!Actor)
		{
			return false;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || Component->IsPendingKill())
			{
				continue;
			}

			OutComponents.push_back(Component);
		}

		std::sort(
			OutComponents.begin(),
			OutComponents.end(),
			[](const UActorComponent* A, const UActorComponent* B)
			{
				if (!A || !B)
				{
					return A < B;
				}
				return A->UUID < B->UUID;
			});

		return !OutComponents.empty();
	}

	UActorComponent* FindOwnedComponentByUUID(const TSet<UActorComponent*>& OwnedComponents, uint32 ComponentUUID)
	{
		if (ComponentUUID == 0)
		{
			return nullptr;
		}

		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component && Component->UUID == ComponentUUID)
			{
				return Component;
			}
		}

		return nullptr;
	}

	UActorComponent* FindOwnedComponentByNameAndClass(
		const TSet<UActorComponent*>& OwnedComponents,
		const FString& ComponentName,
		const UClass* ComponentClass)
	{
		if (!ComponentClass)
		{
			return nullptr;
		}

		for (UActorComponent* Component : OwnedComponents)
		{
			if (!Component || Component->GetClass() != ComponentClass)
			{
				continue;
			}

			if (ComponentName.empty() || Component->GetName() == ComponentName)
			{
				return Component;
			}
		}

		return nullptr;
	}

	USceneComponent* FindOwnedSceneComponentByUUID(const TSet<UActorComponent*>& OwnedComponents, uint32 ComponentUUID)
	{
		if (UActorComponent* Component = FindOwnedComponentByUUID(OwnedComponents, ComponentUUID))
		{
			if (Component->IsA(USceneComponent::StaticClass()))
			{
				return static_cast<USceneComponent*>(Component);
			}
		}

		return nullptr;
	}

}

ULevel* AActor::GetLevel() const { return Level; }
void AActor::SetLevel(ULevel* InLevel) { Level = InLevel; }
UWorld* AActor::GetWorld() const
{
	if (Level)
	{
		return Level->GetTypedOuter<UWorld>();
	}
	return nullptr;
}
USceneComponent* AActor::GetRootComponent() const { return RootComponent; }

void AActor::SetRootComponent(USceneComponent* InRootComponent)
{
	// 의문점
	// 기존에 RootComponent가 있을 시에는 RootComponent의 OwnerActor를 지워주나?
	// 이러면 두 개의 RootComponent가 하나의 Owner을 가지고 있는건데.
	RootComponent = InRootComponent;
	if (RootComponent)
	{
		RootComponent->SetOwner(this);
	}
}

const TSet<UActorComponent*>& AActor::GetComponents() const { return OwnedComponents; }

void AActor::AddOwnedComponent(UActorComponent* InComponent)
{
	if (InComponent == nullptr)
	{
		return;
	}

	auto It = std::find(OwnedComponents.begin(), OwnedComponents.end(), InComponent);
	if (It != OwnedComponents.end())
	{
		return;
	}

	OwnedComponents.insert(InComponent);
	InComponent->SetOwner(this);

	if (RootComponent == nullptr && InComponent->IsA(USceneComponent::StaticClass()))
	{
		RootComponent = static_cast<USceneComponent*>(InComponent);
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* InComponent)
{
	if (InComponent == nullptr)
	{
		return;
	}

	OwnedComponents.erase(InComponent);

	if (RootComponent == InComponent)
	{
		RootComponent = nullptr;
	}

	InComponent->SetOwner(nullptr);
}

bool AActor::CanDeleteInstanceComponent(const UActorComponent* InComponent) const
{
	if (!InComponent || !InComponent->IsInstanceComponent() || !IsComponentOwnedByActor(this, InComponent))
	{
		return false;
	}

	if (!InComponent->IsA(USceneComponent::StaticClass()))
	{
		return true;
	}

	return !HasNonInstanceSceneDescendant(static_cast<const USceneComponent*>(InComponent));
}

bool AActor::DestroyInstanceComponent(UActorComponent* InComponent)
{
	if (!CanDeleteInstanceComponent(InComponent))
	{
		return false;
	}

	TArray<UActorComponent*> ComponentsToDelete;
	if (InComponent->IsA(USceneComponent::StaticClass()))
	{
		GatherSceneDeletionSubtree(static_cast<USceneComponent*>(InComponent), ComponentsToDelete);
	}
	else
	{
		ComponentsToDelete.push_back(InComponent);
	}

	for (UActorComponent* ComponentToDelete : ComponentsToDelete)
	{
		if (!ComponentToDelete || !IsComponentOwnedByActor(this, ComponentToDelete))
		{
			continue;
		}

		if (ComponentToDelete->IsA(USceneComponent::StaticClass()))
		{
			static_cast<USceneComponent*>(ComponentToDelete)->DetachFromParent();
		}

		if (ComponentToDelete->HasBegunPlay())
		{
			ComponentToDelete->EndPlay();
		}

		if (ComponentToDelete->IsRegistered())
		{
			ComponentToDelete->OnUnregister();
		}

		RemoveOwnedComponent(ComponentToDelete);
		ComponentToDelete->MarkPendingKill();
	}

	if (!RootComponent)
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component && Component->IsA(USceneComponent::StaticClass()))
			{
				RootComponent = static_cast<USceneComponent*>(Component);
				break;
			}
		}
	}

	if (Level)
	{
		Level->MarkSpatialDirty();
	}

	return true;
}

void AActor::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	AActor* DuplicatedActor = static_cast<AActor*>(DuplicatedObject);
	DuplicatedActor->Level = nullptr;
	DuplicatedActor->RootComponent = nullptr;
	DuplicatedActor->OwnedComponents.clear();
	DuplicatedActor->bCanEverTick = bCanEverTick;
	DuplicatedActor->bTickEnabled = bTickEnabled;
	DuplicatedActor->bActorBegunPlay = false;
	DuplicatedActor->bPendingDestroy = false;
	DuplicatedActor->bVisible = bVisible;
	DuplicatedActor->bTickInEditor = bTickInEditor;
}

void AActor::DuplicateSubObjects(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	AActor* DuplicatedActor = static_cast<AActor*>(DuplicatedObject);

	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Component || Component->IsPendingKill())
		{
			continue;
		}

		UActorComponent* DuplicatedComponent = static_cast<UActorComponent*>(
			Component->Duplicate(DuplicatedActor, Component->GetName(), Context));
		if (!DuplicatedComponent)
		{
			continue;
		}

		DuplicatedActor->AddOwnedComponent(DuplicatedComponent);
	}
}

void AActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor* DuplicatedActor = static_cast<AActor*>(DuplicatedObject);
	DuplicatedActor->Level = Context.FindDuplicate(Level.Get());
	DuplicatedActor->RootComponent = nullptr;

	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Component)
		{
			continue;
		}

		if (UActorComponent* DuplicatedComponent = Context.FindDuplicate(Component))
		{
			Component->FixupDuplicatedReferences(DuplicatedComponent, Context);
		}
	}

	if (USceneComponent* DuplicatedRootComponent = Context.FindDuplicate(RootComponent))
	{
		DuplicatedActor->SetRootComponent(DuplicatedRootComponent);
	}
}

void AActor::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor* DuplicatedActor = static_cast<AActor*>(DuplicatedObject);

	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Component)
		{
			continue;
		}

		UActorComponent* DuplicatedComponent = Context.FindDuplicate(Component);
		if (!DuplicatedComponent)
		{
			continue;
		}

		if (!DuplicatedComponent->IsRegistered())
		{
			DuplicatedComponent->OnRegister();
		}

		Component->PostDuplicate(DuplicatedComponent, Context);
	}
}

void AActor::PostSpawnInitialize()
{
	if (GetComponentByClass<UUUIDBillboardComponent>() == nullptr)
	{
		UUUIDBillboardComponent* UUIDComponent =
			FObjectFactory::ConstructObject<UUUIDBillboardComponent>(this, "UUIDBillboard");

		if (UUIDComponent)
		{
			AddOwnedComponent(UUIDComponent);

			UUIDComponent->SetWorldOffset(FVector(0.0f, 0.0f, 0.3f));
			UUIDComponent->SetWorldScale(0.3f);
			UUIDComponent->SetTextColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component && !Component->IsRegistered())
		{
			Component->OnRegister();
		}
		if (Component && Component->IsA(UPrimitiveComponent::StaticClass()))
		{
			UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(Component);
			PrimComp->UpdateBounds();
		}
	}
}

void AActor::BeginPlay()
{
	if (bActorBegunPlay)
	{
		return;
	}

	bActorBegunPlay = true;

	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component && !Component->HasBegunPlay())
		{
			Component->BeginPlay();
		}
	}
}

void AActor::Tick(float DeltaTime)
{
	if (!CanTick() || bPendingDestroy)
	{
		return;
	}

	UWorld* World = GetWorld();
	const EWorldType WorldType = World ? World->GetWorldType() : EWorldType::Game;
	const bool bIsPlayWorld = (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
	if (bIsPlayWorld && !bActorBegunPlay)
	{
		return;
	}

	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Component || !Component->CanTick())
		{
			continue;
		}

		if (bIsPlayWorld && !Component->HasBegunPlay())
		{
			continue;
		}

		if (WorldType == EWorldType::Editor && !Component->IsTickInEditor())
		{
			continue;
		}

		Component->Tick(DeltaTime);
	}
}

void AActor::EndPlay()
{
	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component && Component->HasBegunPlay())
		{
			Component->EndPlay();
		}
	}

	bActorBegunPlay = false;
}

void AActor::Destroy()
{
	if (bPendingDestroy)
	{
		return;
	}

	bPendingDestroy = true;
	MarkPendingKill();

	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp)
		{
			Comp->MarkPendingKill();
		}
	}
}

void AActor::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		FString ClassName = GetClass()->GetName();
		Ar.Serialize("Class", ClassName);
		Ar.Serialize("UUID", UUID);

		uint32 RootCompUUID = RootComponent ? RootComponent->UUID : 0;
		Ar.Serialize("RootComponentUUID", RootCompUUID);

		TArray<UActorComponent*> SerializableComponents;
		GatherSerializableOwnedComponents(this, SerializableComponents);

		TArray<FArchive*> ComponentArchives;
		ComponentArchives.reserve(SerializableComponents.size());

		for (UActorComponent* Component : SerializableComponents)
		{
			if (!Component)
			{
				continue;
			}

			FArchive* ComponentArchive = new FArchive(true);

			FString ComponentClassName = Component->GetClass()->GetName();
			FString ComponentName = Component->GetName();
			ComponentArchive->Serialize("Class", ComponentClassName);
			ComponentArchive->Serialize("Name", ComponentName);

			uint32 AttachParentUUID = 0;
			if (Component->IsA(USceneComponent::StaticClass()))
			{
				if (USceneComponent* AttachParent = static_cast<USceneComponent*>(Component)->GetAttachParent())
				{
					AttachParentUUID = AttachParent->UUID;
				}
			}
			ComponentArchive->Serialize("AttachParentUUID", AttachParentUUID);

			Component->Serialize(*ComponentArchive);
			ComponentArchives.push_back(ComponentArchive);
		}

		Ar.Serialize("Components", ComponentArchives);

		for (FArchive* ComponentArchive : ComponentArchives)
		{
			delete ComponentArchive;
		}
	}
	else
	{
		if (Ar.Contains("UUID"))
		{
			uint32 SavedUUID = 0;
			Ar.Serialize("UUID", SavedUUID);

			GUUIDToObjectMap.erase(UUID);
			if (auto It = GUUIDToObjectMap.find(SavedUUID); It != GUUIDToObjectMap.end() && It->second != this)
			{
				It->second->UUID = 0;
				GUUIDToObjectMap.erase(It);
			}
			UUID = SavedUUID;
			GUUIDToObjectMap[SavedUUID] = this;
		}

		for (UActorComponent* ExistingComponent : OwnedComponents)
		{
			if (ExistingComponent)
			{
				ExistingComponent->SetOwner(this);
			}
		}

		uint32 SavedRootCompUUID = 0;
		if (Ar.Contains("RootComponentUUID"))
		{
			Ar.Serialize("RootComponentUUID", SavedRootCompUUID);
		}

		TArray<FArchive*> ComponentArchives;
		if (Ar.Contains("Components"))
		{
			Ar.Serialize("Components", ComponentArchives);
		}

		TArray<USceneComponent*> ComponentsToDetach;
		TArray<std::pair<USceneComponent*, uint32>> PendingAttachments;
		ComponentsToDetach.reserve(ComponentArchives.size());
		PendingAttachments.reserve(ComponentArchives.size());

		for (FArchive* ComponentArchive : ComponentArchives)
		{
			if (!ComponentArchive || !ComponentArchive->Contains("Class"))
			{
				continue;
			}

			FString ComponentClassName;
			ComponentArchive->Serialize("Class", ComponentClassName);

			FString ComponentName;
			if (ComponentArchive->Contains("Name"))
			{
				ComponentArchive->Serialize("Name", ComponentName);
			}

			uint32 SavedComponentUUID = 0;
			if (ComponentArchive->Contains("UUID"))
			{
				ComponentArchive->Serialize("UUID", SavedComponentUUID);
			}

			uint32 SavedAttachParentUUID = 0;
			if (ComponentArchive->Contains("AttachParentUUID"))
			{
				ComponentArchive->Serialize("AttachParentUUID", SavedAttachParentUUID);
			}

			UClass* ComponentClass = UClass::FindClass(ComponentClassName);
			if (!ComponentClass)
			{
				UE_LOG("[Serialize] Unknown Component Class: %s", ComponentClassName.c_str());
				continue;
			}

			UActorComponent* TargetComponent = FindOwnedComponentByUUID(OwnedComponents, SavedComponentUUID);
			if (!TargetComponent)
			{
				TargetComponent = FindOwnedComponentByNameAndClass(OwnedComponents, ComponentName, ComponentClass);
			}

			if (!TargetComponent)
			{
				UObject* NewObject = FObjectFactory::ConstructObject(ComponentClass, this, ComponentName.empty() ? ComponentClassName : ComponentName);
				TargetComponent = static_cast<UActorComponent*>(NewObject);

				if (TargetComponent)
				{
					AddOwnedComponent(TargetComponent);
				}
			}

			if (!TargetComponent)
			{
				continue;
			}

			TargetComponent->Serialize(*ComponentArchive);

			if (TargetComponent->IsA(USceneComponent::StaticClass()))
			{
				USceneComponent* SceneComponent = static_cast<USceneComponent*>(TargetComponent);
				ComponentsToDetach.push_back(SceneComponent);
				PendingAttachments.push_back({ SceneComponent, SavedAttachParentUUID });
			}
		}

		for (USceneComponent* SceneComponent : ComponentsToDetach)
		{
			if (SceneComponent)
			{
				SceneComponent->DetachFromParent();
			}
		}

		for (const auto& PendingAttachment : PendingAttachments)
		{
			USceneComponent* SceneComponent = PendingAttachment.first;
			const uint32 AttachParentUUID = PendingAttachment.second;
			if (!SceneComponent)
			{
				continue;
			}

			if (AttachParentUUID == 0)
			{
				continue;
			}

			if (USceneComponent* AttachParent = FindOwnedSceneComponentByUUID(OwnedComponents, AttachParentUUID))
			{
				SceneComponent->AttachTo(AttachParent);
			}
		}

		RootComponent = FindOwnedSceneComponentByUUID(OwnedComponents, SavedRootCompUUID);
		if (!RootComponent)
		{
			for (UActorComponent* Comp : OwnedComponents)
			{
				if (Comp && Comp->IsA(USceneComponent::StaticClass()) && static_cast<USceneComponent*>(Comp)->GetAttachParent() == nullptr)
				{
					RootComponent = static_cast<USceneComponent*>(Comp);
					break;
				}
			}
		}

		if (RootComponent)
		{
			SetRootComponent(RootComponent);
		}

		for (UActorComponent* Component : OwnedComponents)
		{
			if (!Component)
			{
				continue;
			}

			if (!Component->IsRegistered())
			{
				Component->OnRegister();
			}

			if (Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				static_cast<UPrimitiveComponent*>(Component)->UpdateBounds();
			}
		}

		for (FArchive* ComponentArchive : ComponentArchives)
		{
			delete ComponentArchive;
		}
	}
}
const FVector& AActor::GetActorLocation() const
{
	if (RootComponent == nullptr)
	{
		return GZeroVector;
	}

	return RootComponent->GetRelativeLocation();
}

void AActor::SetActorLocation(const FVector& InLocation)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	RootComponent->SetRelativeLocation(InLocation);
}


const FTransform& AActor::GetActorTransform() const
{
	if (RootComponent == nullptr)
	{
		return FTransform{};
	}

	return RootComponent->GetRelativeTransform();
}

void AActor::SetActorTransform(const FTransform& InTransform)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	RootComponent->SetRelativeTransform(InTransform);
}
