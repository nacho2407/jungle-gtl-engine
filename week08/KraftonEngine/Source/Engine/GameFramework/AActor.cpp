#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Math/Rotator.h"
#include "GameFramework/World.h"
#include "Serialization/Archive.h"

#include <algorithm>

IMPLEMENT_CLASS(AActor, UObject)

AActor::~AActor()
{
	for (auto* Comp : OwnedComponents)
	{
		Comp->DestroyRenderState();
		UObjectManager::Get().DestroyObject(Comp);
	}

	OwnedComponents.clear();
	RootComponent = nullptr;
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class)
{
	if (!Class) return nullptr;

	UObject* Obj = FObjectFactory::Get().Create(Class->name, this);
	if (!Obj) return nullptr;

	UActorComponent* Comp = Cast<UActorComponent>(Obj);
	if (!Comp) {
		UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	Comp->SetOwner(this);
	OwnedComponents.push_back(Comp);
	bPrimitiveCacheDirty = true;
	Comp->CreateRenderState();
	MarkPickingDirty();
	return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp)
{
	if (!Comp) return;

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
	if (it == OwnedComponents.end()) {
		Comp->SetOwner(this);
		Comp->SetOuter(this);
		OwnedComponents.push_back(Comp);
		bPrimitiveCacheDirty = true;
		MarkPickingDirty();
		Comp->CreateRenderState();
	}
}

void AActor::RemoveComponent(UActorComponent* Component)
{
	if (!Component) return;

	Component->DestroyRenderState();

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
	if (it != OwnedComponents.end()) {
		OwnedComponents.erase(it);
		bPrimitiveCacheDirty = true;
		MarkPickingDirty();
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

UWorld* AActor::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}

void AActor::SetVisible(bool Visible)
{
	if (bVisible == Visible)
	{
		return;
	}

	bVisible = Visible;
	if (UWorld* World = GetWorld())
	{
		World->MarkWorldPrimitivePickingBVHDirty();
		World->UpdateActorInOctree(this);
	}
	for (UPrimitiveComponent* Prim : GetPrimitiveComponents())
	{
		if (Prim)
		{
			Prim->MarkProxyDirty(EDirtyFlag::Visibility);
		}
	}
}

void AActor::MarkPickingDirty()
{
	if (UWorld* World = GetWorld())
	{
		World->MarkWorldPrimitivePickingBVHDirty();
	}
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

void AActor::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	// 소유 포인터(OwnedComponents/RootComponent/Outer)는 직렬화 제외 — 복제 단계에서 재구성.
	Ar << bVisible;
	Ar << bNeedsTick;
}

UObject* AActor::Duplicate(UObject* NewOuter) const
{
	// 1) 같은 타입 액터를 팩토리로 생성 (UObject::Duplicate 경유: Serialize 왕복까지 수행)
	//    NewOuter 미지정 시 원본의 Outer(World)를 승계 → 이후 AddActor가 다시 보강.
	UObject* DupBase = UObject::Duplicate(NewOuter);
	AActor* Dup = static_cast<AActor*>(DupBase);
	if (!Dup)
	{
		return nullptr;
	}

	// 2) 얕은 복사로 따라온 컴포넌트 컨테이너 즉시 비우기 (UObject::Serialize는 컴포넌트를 다루지 않으므로
	//    실제로는 비어있지만, 향후 수정을 대비한 안전장치)
	Dup->OwnedComponents.clear();
	Dup->RootComponent = nullptr;
	Dup->bPrimitiveCacheDirty = true;

	// 3) 컴포넌트들을 복제 (현 단계에서는 SceneComponent 트리 깊이 1 가정 — Root + 형제들)
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (!Comp) continue;

		UObject* DupCompBase = Comp->Duplicate(Dup);
		UActorComponent* DupComp = Cast<UActorComponent>(DupCompBase);
		if (!DupComp) continue;

		DupComp->SetOwner(Dup);

		if (Comp == RootComponent)
		{
			USceneComponent* DupRoot = Cast<USceneComponent>(DupComp);
			Dup->SetRootComponent(DupRoot);
		}

		Dup->OwnedComponents.push_back(DupComp);
		DupComp->CreateRenderState();
	}

	Dup->bPrimitiveCacheDirty = true;

	// 4) 월드에 등록
	if (UWorld* World = GetWorld())
	{
		World->AddActor(Dup);
	}

	return Dup;
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
