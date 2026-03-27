#include "Actor.h"
#include "Object/ObjectFactory.h"
#include "Component/UUIDBillboardComponent.h"
#include "Object/Class.h"
#include "Renderer/Material.h"
#include "Component/TextComponent.h"
#include "Component/SceneComponent.h"
#include "Serializer/Archive.h"
#include "Scene/Scene.h"
IMPLEMENT_RTTI(AActor, UObject)

namespace {
	FVector GZeroVector{};
}

UScene* AActor::GetScene() const { return Scene; }
void AActor::SetScene(UScene* InScene) { Scene = InScene; }
UWorld* AActor::GetWorld() const
{
	if (Scene)
	{
		return Scene->GetTypedOuter<UWorld>();
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

const TArray<UActorComponent*>& AActor::GetComponents() const { return OwnedComponents; }

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

	OwnedComponents.push_back(InComponent);
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

	std::erase(OwnedComponents, InComponent);

	if (RootComponent == InComponent)
	{
		RootComponent = nullptr;
	}

	InComponent->SetOwner(nullptr);
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

	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component && Component->CanTick())
		{
			Component->Tick(DeltaTime);
		}
	}
}

void AActor::EndPlay()
{
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

		TArray<uint32> CompUUIDs;
		for (UActorComponent* Comp : GetComponents())
			if (Comp) CompUUIDs.push_back(Comp->UUID);
		Ar.SerializeUIntArray("ComponentUUIDs", CompUUIDs);

		if (USceneComponent* Root = GetRootComponent())
		{
			const FTransform Transform = Root->GetRelativeTransform();

			FVector Location = Transform.GetTranslation();
			FVector Rotation = Transform.Rotator().Euler();
			FVector Scale = Transform.GetScale3D();
			Ar.Serialize("Location", Location);
			Ar.Serialize("Rotation", Rotation);
			Ar.Serialize("Scale", Scale);
		}
		if (UPrimitiveComponent* PrimComp = GetComponentByClass<UPrimitiveComponent>())
		{
			if (PrimComp->GetMaterial() && !PrimComp->GetMaterial()->GetOriginName().empty())
			{
				FString MatName = PrimComp->GetMaterial()->GetOriginName();
				Ar.Serialize("Material", MatName);
			}

			if (PrimComp->GetPrimitive())
			{
				FString PrimFileName = PrimComp->GetPrimitiveFileName();
				Ar.Serialize("PrimitiveFileName", PrimFileName);
			}
		}
		if (UTextComponent* TC = GetComponentByClass<UTextComponent>())
		{
			FString Text = TC->GetText();
			FVector4 Color4 = TC->GetTextColor();
			bool bBillboard = TC->IsBillboard();
			Ar.Serialize("Text", Text);
			Ar.Serialize("TextColor", Color4);
			Ar.Serialize("Billboard", bBillboard);
		
		}
	}
	else//Load 
	{
		if (Ar.Contains("UUID"))
		{
			uint32 SavedUUID = 0;
			Ar.Serialize("UUID", SavedUUID);
			// 기존 UUID 제거
			GUUIDToObjectMap.erase(UUID);
			// 충돌하는 UUID가 이미 있으면 기존 것 제거
			if (auto It = GUUIDToObjectMap.find(SavedUUID); It != GUUIDToObjectMap.end() && It->second != this)
			{
				It->second->UUID = 0;
				GUUIDToObjectMap.erase(It);
			}
			UUID = SavedUUID;
			GUUIDToObjectMap[SavedUUID] = this;

		}

		//restore Transform
		FTransform Transform;
		if (Ar.Contains("Location"))
		{
			FVector Location;
			Ar.Serialize("Location", Location);
			Transform.SetTranslation(Location);
		}
		if (Ar.Contains("Rotation"))
		{
			FVector Rotation;
			Ar.Serialize("Rotation", Rotation);
			Transform.SetRotation(FRotator::MakeFromEuler(Rotation));
		}
		if (Ar.Contains("Scale"))
		{
			FVector Scale;
			Ar.Serialize("Scale", Scale);
			Transform.SetScale3D(Scale);
		}
		if (USceneComponent* Root = GetRootComponent())
			Root->SetRelativeTransform(Transform);

		// Components UUID Restore
		if (Ar.Contains("ComponentUUIDs"))
		{
			TArray<uint32> CompUUIDs;
			Ar.SerializeUIntArray("ComponentUUIDs", CompUUIDs);
			const TArray<UActorComponent*>& Components = GetComponents();
			for (size_t i = 0; i < CompUUIDs.size(); i++)
			{
				GUUIDToObjectMap.erase(Components[i]->UUID);
				if (auto It = GUUIDToObjectMap.find(CompUUIDs[i]);
					It != GUUIDToObjectMap.end() &&
					It->second != Components[i])
				{
					It->second->UUID = 0;
					GUUIDToObjectMap.erase(It);
				}
				Components[i]->UUID = CompUUIDs[i];
				GUUIDToObjectMap[CompUUIDs[i]] = Components[i];
			}
		}
		//Setting Owner
		for (UActorComponent* Comp : GetComponents())
			if (Comp)
				Comp->SetOwner(this);
		if (UTextComponent* TC = GetComponentByClass<UTextComponent>())
		{
			FString Text = TC->GetText();
			FVector4 TextColor = TC->GetTextColor();
			bool bBillboard = TC->IsBillboard();

			Ar.Serialize("Text", Text);
			Ar.Serialize("TextColor", TextColor);
			Ar.Serialize("Billboard", bBillboard);

			TC->SetText(Text);
			TC->SetTextColor(TextColor);
			TC->SetBillboard(bBillboard);
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
