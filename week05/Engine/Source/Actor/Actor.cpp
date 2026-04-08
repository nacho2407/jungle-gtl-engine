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
	if (Ar.IsSaving())// Save Actor property
	{
		FString ClassName = GetClass()->GetName();
		Ar.Serialize("Class", ClassName);
		Ar.Serialize("UUID", UUID);

		uint32 RootCompUUID = RootComponent ? RootComponent->UUID : 0;
		Ar.Serialize("RootComponentUUID", RootCompUUID);

		TArray<FArchive*> ComponentArchives;
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component)
			{
				FArchive* ComponentArchive = new FArchive(true);
				
				FString ComponentClassName = Component->GetClass()->GetName();
				ComponentArchive->Serialize("Class", ComponentClassName);

				Component->Serialize(*ComponentArchive);
				ComponentArchives.push_back(ComponentArchive);
			}
		}

		Ar.Serialize("Components", ComponentArchives);

		for (FArchive* ComponentArchive : ComponentArchives) delete ComponentArchive;
	}
	else//Load 
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

		uint32 SavedRootCompUUID = 0;
		if (Ar.Contains("RootComponentUUID"))
		{
			Ar.Serialize("RootComponentUUID", SavedRootCompUUID);
		}
		
		if (Ar.Contains("Components"))
		{
			TArray<FArchive*> ComponentArchives;
			Ar.Serialize("Components", ComponentArchives);

			for (FArchive* ComponentArchive: ComponentArchives)
			{
				if (ComponentArchive->Contains("Class"))
				{
					FString ComponentClassName;
					ComponentArchive->Serialize("Class", ComponentClassName);

					
					UClass* ComponentClass = UClass::FindClass(ComponentClassName);
					if (ComponentClass)
					{
						UActorComponent* TargetComponent = nullptr;

						for (UActorComponent* ExistingComponent : OwnedComponents)
						{
							if (ExistingComponent->GetClass() == ComponentClass)
							{
								TargetComponent = ExistingComponent;
								break;
							}
						}

						if (!TargetComponent)
						{
							UObject* NewObject = FObjectFactory::ConstructObject(ComponentClass, this);
							TargetComponent = static_cast<UActorComponent*>(NewObject);

							if (TargetComponent)
							{
								AddOwnedComponent(TargetComponent);
							}
						}

						if (TargetComponent)
						{
							TargetComponent->Serialize(*ComponentArchive);
						}
					}
					else
					{
						UE_LOG("[Serialize] Unknown Component Class: %s", ComponentClassName.c_str());
					}
				}
			}
			for (FArchive* ComponentArchive : ComponentArchives) delete ComponentArchive;
		}

		if (SavedRootCompUUID != 0)
		{
			for (UActorComponent* Comp : OwnedComponents)
			{
				if (Comp && Comp->UUID == SavedRootCompUUID && Comp->IsA(USceneComponent::StaticClass()))
				{
					RootComponent = static_cast<USceneComponent*>(Comp);
					break;
				}
			}
		}
		if (RootComponent)
		{
			for (UActorComponent* Comp : OwnedComponents)
			{
				if (Comp != RootComponent && Comp->IsA(USceneComponent::StaticClass()))
				{
					USceneComponent* SceneComp = static_cast<USceneComponent*>(Comp);
					SceneComp->AttachTo(RootComponent);
				}
			}
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
