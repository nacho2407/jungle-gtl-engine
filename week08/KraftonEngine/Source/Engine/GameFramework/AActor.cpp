#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Math/Rotator.h"

IMPLEMENT_CLASS(AActor, UObject)

AActor::~AActor()
{
	for (auto* Comp : OwnedComponents)
	{
		UObjectManager::Get().DestroyObject(Comp);
	}

	OwnedComponents.clear();
	RootComponent = nullptr;
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class)
{
	if (!Class) return nullptr;

	UObject* Obj = FObjectFactory::Get().Create(Class->name);
	if (!Obj) return nullptr;

	UActorComponent* Comp = Cast<UActorComponent>(Obj);
	if (!Comp) {
		UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	Comp->SetOwner(this);
	OwnedComponents.push_back(Comp);
	bPrimitiveCacheDirty = true;
	return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp)
{
	if (!Comp) return;

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
	if (it == OwnedComponents.end()) {
		Comp->SetOwner(this);
		OwnedComponents.push_back(Comp);
		bPrimitiveCacheDirty = true;
	}
}

void AActor::RemoveComponent(UActorComponent* Component)
{
	if (!Component) return;

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
	if (it != OwnedComponents.end()) {
		OwnedComponents.erase(it);
		bPrimitiveCacheDirty = true;
	}

	// RootComponent가 제거되면 nullptr로
	if (RootComponent == Component)
		RootComponent = nullptr;

	UObjectManager::Get().DestroyObject(Component);
}

void AActor::SetRootComponent(USceneComponent* Comp)
{
	if (!Comp) return;
	RootComponent = Comp;
}

FVector AActor::GetActorLocation() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldLocation();
	}
	return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
	PendingActorLocation = NewLocation;

	if (RootComponent)
	{
		RootComponent->SetWorldLocation(NewLocation);
	}
}

void AActor::AddActorWorldOffset(const FVector& Delta)
{
	if (RootComponent)
	{
		RootComponent->AddWorldOffset(Delta);
	}
}

void AActor::Tick(float DeltaTime)
{
	for (UActorComponent* ActorComp : OwnedComponents)
	{
		ActorComp->Tick(DeltaTime);
	}
}

FRotator AActor::GetActorRotation() const
{
	return RootComponent ? RootComponent->GetRelativeRotation() : FRotator();
}

void AActor::SetActorRotation(const FRotator& NewRotation)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(NewRotation);
	}
}

void AActor::SetActorRotation(const FVector& EulerRotation)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(EulerRotation);
	}
}

FVector AActor::GetActorScale() const
{
	return RootComponent ? RootComponent->GetRelativeScale() : FVector(1, 1, 1);
}

void AActor::SetActorScale(const FVector& NewScale)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeScale(NewScale);
	}
}

FVector AActor::GetActorForward() const
{
	if (RootComponent)
	{
		return RootComponent->GetForwardVector();
	}

	return FVector(0, 0, 1);
}

const TArray<UPrimitiveComponent*>& AActor::GetPrimitiveComponents() const
{
	if (bPrimitiveCacheDirty)
	{
		PrimitiveCache.clear();
		for (UActorComponent* Comp : OwnedComponents)
		{
			if (Comp && Comp->IsA<UPrimitiveComponent>())
			{
				PrimitiveCache.emplace_back(static_cast<UPrimitiveComponent*>(Comp));
			}
		}
		bPrimitiveCacheDirty = false;
	}
	return PrimitiveCache;
}
