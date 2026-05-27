#include "GameFramework/AActor.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Math/Rotator.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "Serialization/Archive.h"
#include "Serialization/DuplicateArchive.h"

#include <algorithm>

#include "Object/GarbageCollection.h"

namespace
{
	void AddUniqueComponent(TArray<UActorComponent*>& Components, TSet<UActorComponent*>& Seen, UActorComponent* Component)
	{
		if (!IsAliveObject(Component))
		{
			return;
		}

		if (Seen.insert(Component).second)
		{
			Components.push_back(Component);
		}
	}

	void GatherSceneComponentSubtree(USceneComponent* Root, TArray<UActorComponent*>& Components, TSet<UActorComponent*>& Seen)
	{
		if (!IsAliveObject(Root))
		{
			return;
		}

		AddUniqueComponent(Components, Seen, Root);
		for (USceneComponent* Child : Root->GetChildren())
		{
			GatherSceneComponentSubtree(Child, Components, Seen);
		}
	}

	bool ContainsComponent(const TArray<UActorComponent*>& Components, const UActorComponent* Component)
	{
		return std::find(Components.begin(), Components.end(), Component) != Components.end();
	}

}

AActor::AActor()
{
	PrimaryActorTick.SetTarget(this);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEnabled = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}
AActor::~AActor()
{
	PrimaryActorTick.UnRegisterTickFunction();

    OwnedComponents.clear();
    PrimitiveCache.clear();
	RootComponent = nullptr;
}


bool AActor::OwnsComponent(const UActorComponent* Comp) const
{
	if (!Comp)
	{
		return false;
	}

	return std::find_if(
		OwnedComponents.begin(),
		OwnedComponents.end(),
		[Comp](const TObjectPtr<UActorComponent>& ExistingComponent)
		{
			return ExistingComponent.GetRaw() == Comp;
		}) != OwnedComponents.end();
}


bool AActor::CanRegisterComponent(UActorComponent* Comp) const
{
	if (!IsValid(Comp))
	{
		return false;
	}

	AActor* ExistingOwner = Comp->GetOwnerEvenIfPendingKill();
	if (ExistingOwner && ExistingOwner != this)
	{
		return false;
	}

	return true;
}


void AActor::OnComponentBeingDestroyed(UActorComponent* Component)
{
	if (!Component || Component->GetOwnerEvenIfPendingKill() != this)
	{
		return;
	}

	TArray<UActorComponent*> ComponentsToDetach;
	TSet<UActorComponent*> Seen;
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		GatherSceneComponentSubtree(SceneComponent, ComponentsToDetach, Seen);
	}
	else
	{
		AddUniqueComponent(ComponentsToDetach, Seen, Component);
	}

	if (ComponentsToDetach.empty())
	{
		return;
	}

	if (ContainsComponent(ComponentsToDetach, RootComponent.GetRaw()))
	{
		RootComponent = nullptr;
	}

	for (UActorComponent* ExistingComponent : OwnedComponents)
	{
		if (!IsAliveObject(ExistingComponent))
		{
			continue;
		}

		UMovementComponent* MovementComponent = Cast<UMovementComponent>(ExistingComponent);
		if (!MovementComponent || ContainsComponent(ComponentsToDetach, MovementComponent))
		{
			continue;
		}

		for (UActorComponent* DetachedComponent : ComponentsToDetach)
		{
			if (USceneComponent* DetachedSceneComponent = Cast<USceneComponent>(DetachedComponent))
			{
				MovementComponent->ClearUpdatedComponentIfMatches(DetachedSceneComponent);
			}
		}
	}

	OwnedComponents.erase(
		std::remove_if(
			OwnedComponents.begin(),
			OwnedComponents.end(),
			[&ComponentsToDetach](const TObjectPtr<UActorComponent>& ExistingComponent)
			{
				return !IsAliveObject(ExistingComponent.GetRaw()) || ContainsComponent(ComponentsToDetach, ExistingComponent.GetRaw());
			}),
		OwnedComponents.end());

	bPrimitiveCacheDirty = true;
	PrimitiveCache.clear();
	MarkPickingDirty();
}


UActorComponent* AActor::AddComponentByClass(UClass* Class)
{
	if (!Class) return nullptr;

	UObject* Obj = FObjectFactory::Get().Create(Class->GetName(), this);
	if (!Obj) return nullptr;

	UActorComponent* Comp = Cast<UActorComponent>(Obj);
	if (!Comp)
	{
		UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	RegisterComponent(Comp);
	if (!IsValid(Comp) || Comp->GetOwner() != this)
	{
		if (IsAliveObject(Comp))
		{
			Comp->RouteComponentDestroyed();
			UObjectManager::Get().DestroyObject(Comp);
		}
		return nullptr;
	}

	return Comp;
}


void AActor::RegisterComponent(UActorComponent* Comp)
{
	if (!CanRegisterComponent(Comp))
	{
		return;
	}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Comp))
	{
		USceneComponent* Parent = SceneComponent->GetParent();
		if (IsValid(Parent) && Parent->GetOwner() != this)
		{
			SceneComponent->SetParent(nullptr);
		}
	}

	if (!OwnsComponent(Comp))
	{
		Comp->SetOwner(this);
		Comp->SetOuter(this);
		OwnedComponents.push_back(Comp);
	}
	else if (Comp->GetOwnerEvenIfPendingKill() != this)
	{
		Comp->SetOwner(this);
	}

	bPrimitiveCacheDirty = true;
	PrimitiveCache.clear();
	MarkPickingDirty();
	Comp->CreateRenderState();
}


void AActor::RemoveComponent(UActorComponent* Component)
{
    if (!IsValid(Component) || Component->GetOwner() != this)
    {
        return;
    }

	TArray<UActorComponent*> ComponentsToRemove;
	TSet<UActorComponent*> Seen;
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		GatherSceneComponentSubtree(SceneComponent, ComponentsToRemove, Seen);
	}
	else
	{
		AddUniqueComponent(ComponentsToRemove, Seen, Component);
	}

	if (ComponentsToRemove.empty())
	{
		return;
	}

	if (ContainsComponent(ComponentsToRemove, RootComponent.GetRaw()))
	{
		RootComponent = nullptr;
	}

	for (UActorComponent* ExistingComponent : OwnedComponents)
	{
		if (!IsValid(ExistingComponent))
		{
			continue;
		}

		UMovementComponent* MovementComponent = Cast<UMovementComponent>(ExistingComponent);
		if (!MovementComponent || ContainsComponent(ComponentsToRemove, MovementComponent))
		{
			continue;
		}

		for (UActorComponent* RemovedComponent : ComponentsToRemove)
		{
			if (USceneComponent* RemovedSceneComponent = Cast<USceneComponent>(RemovedComponent))
			{
				MovementComponent->ClearUpdatedComponentIfMatches(RemovedSceneComponent);
			}
		}
	}

	for (UActorComponent* RemovedComponent : ComponentsToRemove)
	{
		if (!IsAliveObject(RemovedComponent))
		{
			continue;
		}

		RemovedComponent->EndPlay();
		RemovedComponent->RouteComponentDestroyed();
	}

	OwnedComponents.erase(
		std::remove_if(
			OwnedComponents.begin(),
			OwnedComponents.end(),
			[&ComponentsToRemove](const TObjectPtr<UActorComponent>& ExistingComponent)
			{
				return !IsValid(ExistingComponent.GetRaw()) || ContainsComponent(ComponentsToRemove, ExistingComponent.GetRaw());
			}),
		OwnedComponents.end());

	bPrimitiveCacheDirty = true;
	PrimitiveCache.clear();
	MarkPickingDirty();

	for (UActorComponent* RemovedComponent : ComponentsToRemove)
	{
		if (IsAliveObject(RemovedComponent))
		{
			UObjectManager::Get().DestroyObject(RemovedComponent);
		}
	}
}

void AActor::SetRootComponent(USceneComponent* Comp)
{
	if (!Comp)
	{
		RootComponent = nullptr;
		return;
	}

	if (!CanRegisterComponent(Comp))
	{
		return;
	}

	if (!OwnsComponent(Comp))
	{
		RegisterComponent(Comp);
	}

	if (IsValid(Comp) && Comp->GetOwner() == this && OwnsComponent(Comp))
	{
		RootComponent = Comp;
	}
}

TArray<UActorComponent*> AActor::GetComponents() const
{
	TArray<UActorComponent*> Result;
	Result.reserve(OwnedComponents.size());
	for (UActorComponent* Component : OwnedComponents)
	{
		if (IsValid(Component))
		{
			Result.push_back(Component);
		}
	}
	return Result;
}

UWorld* AActor::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}

UWorld* AActor::GetWorldEvenIfPendingKill() const
{
	return GetTypedOuterEvenIfPendingKill<UWorld>();
}


ULevel* AActor::GetLevel() const
{
	return GetTypedOuter<ULevel>();
}

void AActor::SetVisible(bool Visible)
{
	if (bVisible == Visible)
	{
		return;
	}

	bVisible = Visible;
	// 각 PrimitiveComponent가 자신의 dirty 시퀀스(Proxy/Octree/PickingBVH/VisibleSet)를
	// 전파하면 액터 단위 캐시도 자연히 무효화된다.
	for (UPrimitiveComponent* Prim : GetPrimitiveComponents())
	{
		if (Prim)
		{
			Prim->MarkRenderVisibilityDirty();
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
	if (USceneComponent* Root = GetRootComponent())
	{
		return Root->GetWorldLocation();
	}
	return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
	PendingActorLocation = NewLocation;

	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetWorldLocation(NewLocation);
	}
}

void AActor::AddActorWorldOffset(const FVector& Delta)
{
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->AddWorldOffset(Delta);
	}
}

void AActor::BeginPlay()
{
	// 재진입 방지 — UE의 HasActorBegunPlay() 가드 대응.
	if (bActorHasBegunPlay) return;
	bActorHasBegunPlay = true;

	// UE 순서: 컴포넌트 BeginPlay 먼저, 그다음 Actor 본인 (오버라이드 측 Super 호출 시).
	for (UActorComponent* Comp : OwnedComponents)
	{
        if (IsValid(Comp))
        {
            Comp->BeginPlay();
        }
	}
}

//엔진 단계에서의 틱
void AActor::TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	if (GetWorld())
	{
		//유저 코드 
		Tick(DeltaSeconds);	
	}
}

void AActor::EndPlay()
{
	if (!bActorHasBegunPlay) return;
	bActorHasBegunPlay = false;
	PrimaryActorTick.UnRegisterTickFunction();

	for (UActorComponent* Comp : OwnedComponents)
	{
        if (IsValid(Comp))
		{
			Comp->PrimaryComponentTick.UnRegisterTickFunction();
			Comp->EndPlay();
		}
	}
}

void AActor::Tick(float DeltaTime)
{
	/*for (UActorComponent* ActorComp : OwnedComponents)
	{
		ActorComp->Tick(DeltaTime);
	}*/
}

FRotator AActor::GetActorRotation() const
{
	if (USceneComponent* Root = GetRootComponent())
	{
		return Root->GetRelativeRotation();
	}
	return FRotator();
}

void AActor::SetActorRotation(const FRotator& NewRotation)
{
	PendingActorRotation = NewRotation;
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeRotation(NewRotation);
	}
}

void AActor::SetActorRotation(const FVector& EulerRotation)
{
	PendingActorRotation = FRotator(EulerRotation);
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeRotation(EulerRotation);
	}
}

FVector AActor::GetActorScale() const
{
	if (USceneComponent* Root = GetRootComponent())
	{
		return Root->GetRelativeScale();
	}
	return FVector(1, 1, 1);
}

void AActor::SetActorScale(const FVector& NewScale)
{
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeScale(NewScale);
	}
}

FVector AActor::GetActorForward() const
{
	if (USceneComponent* Root = GetRootComponent())
	{
		return Root->GetForwardVector();
	}

	return FVector(0, 0, 1);
}

FVector AActor::GetActorRight() const
{
	if (USceneComponent* Root = GetRootComponent())
	{
		return Root->GetRightVector();
	}
	
	return FVector(0, 1, 0);
}

namespace
{
	FString JoinTagsCommaSep(const TArray<FName>& Tags);
	TArray<FName> SplitTagsCommaSep(const FString& In);
}

void AActor::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	// 소유 포인터(OwnedComponents/RootComponent/Outer)는 직렬화 제외 — 복제 단계에서 재구성.
	if (Ar.IsSaving())
	{
		PendingActorLocation = GetActorLocation();
		PendingActorRotation = GetActorRotation();
		PendingActorScale = GetActorScale();
		PendingActorVisible = bVisible;
		PendingTagsString = JoinTagsCommaSep(Tags);
	}

	SerializeProperties(Ar, PF_Save);

	if (Ar.IsLoading())
	{
		SetActorLocation(PendingActorLocation);
		SetActorRotation(PendingActorRotation);
		SetActorScale(PendingActorScale);
		SetVisible(PendingActorVisible);
		SetTags(SplitTagsCommaSep(PendingTagsString));
	}
}

bool AActor::HasTag(const FName& Tag) const
{
	for (const FName& T : Tags)
	{
		if (T == Tag) return true;
	}
	return false;
}

void AActor::AddTag(const FName& Tag)
{
	if (HasTag(Tag)) return;
	Tags.push_back(Tag);
}

void AActor::RemoveTag(const FName& Tag)
{
	for (auto it = Tags.begin(); it != Tags.end(); ++it)
	{
		if (*it == Tag) { Tags.erase(it); return; }
	}
}

namespace
{
	FString JoinTagsCommaSep(const TArray<FName>& Tags)
	{
		FString Result;
		for (size_t i = 0; i < Tags.size(); ++i)
		{
			if (i > 0) Result += ",";
			Result += Tags[i].ToString();
		}
		return Result;
	}

	TArray<FName> SplitTagsCommaSep(const FString& In)
	{
		TArray<FName> Out;
		size_t Start = 0;
		while (Start <= In.size())
		{
			size_t End = In.find(',', Start);
			if (End == FString::npos) End = In.size();

			// 양 끝 공백 trim — "a, b" 같은 입력 허용.
			size_t TokStart = Start;
			size_t TokEnd = End;
			while (TokStart < TokEnd && std::isspace(static_cast<unsigned char>(In[TokStart]))) ++TokStart;
			while (TokEnd > TokStart && std::isspace(static_cast<unsigned char>(In[TokEnd - 1]))) --TokEnd;

			if (TokEnd > TokStart)
			{
				Out.push_back(FName(In.substr(TokStart, TokEnd - TokStart)));
			}

			if (End == In.size()) break;
			Start = End + 1;
		}
		return Out;
	}
}

// SceneComponent 서브트리를 재귀 복제. 부모 → 자식 순으로 만들되,
// RegisterComponent(=CreateRenderState/Proxy) 호출 전에 부모에 Attach 해서
// 프록시가 처음부터 올바른 월드 트랜스폼으로 생성되도록 한다.
// (Attach 전 Register 시 child의 world transform = local로 잘못 계산되어
//  복제 직후 한 프레임 동안 잘못된 위치에 렌더되는 문제가 있었음.)
static USceneComponent* DuplicateSceneSubtree(
	const USceneComponent* Src,
	AActor* DupOwner,
	USceneComponent* DupParent,
	TSet<const UActorComponent*>& Visited,
	FDuplicateArchiveContext& DuplicateContext)
{
	if (!Src) return nullptr;

	USceneComponent* DupNode = Cast<USceneComponent>(Src->DuplicateWithArchiveContext(DupOwner, DuplicateContext));
	if (!DupNode) return nullptr;

	DupNode->SetOwner(DupOwner);
	if (DupParent)
	{
		DupNode->AttachToComponent(DupParent); // Register 전에 부모 연결
	}
	DupOwner->RegisterComponent(DupNode); // Outer/OwnedComponents/CreateRenderState 일괄 처리
	Visited.insert(Src);

	for (USceneComponent* Child : Src->GetChildren())
	{
		DuplicateSceneSubtree(Child, DupOwner, DupNode, Visited, DuplicateContext);
	}
	return DupNode;
}

UObject* AActor::Duplicate(UObject* NewOuter) const
{
	// 1) 같은 타입 액터를 팩토리로 생성 (공유 DuplicateContext로 Serialize 왕복)
	//    NewOuter 미지정 시 원본의 Outer(World)를 승계 → 이후 AddActor가 다시 보강.
	FDuplicateArchiveContext DuplicateContext;
	UObject* DupBase = UObject::DuplicateWithArchiveContext(NewOuter, DuplicateContext);
	AActor* Dup = static_cast<AActor*>(DupBase);
	if (!Dup)
	{
		return nullptr;
	}

	// 2) 얕은 복사로 따라온 컴포넌트 컨테이너 즉시 비우기 (안전장치)
	Dup->OwnedComponents.clear();
	Dup->RootComponent = nullptr;
	Dup->bPrimitiveCacheDirty = true;

	TSet<const UActorComponent*> Visited;

	// 3a) Root 서브트리 재귀 복제 — 도달 가능한 모든 SceneComponent를 처리
	if (USceneComponent* SourceRoot = GetRootComponent())
	{
		USceneComponent* DupRoot = DuplicateSceneSubtree(SourceRoot, Dup, nullptr, Visited, DuplicateContext);
		if (DupRoot)
		{
			Dup->SetRootComponent(DupRoot);
		}
	}

	// 3b) 트리에 포함되지 않은 나머지(비씬 컴포넌트 + 분리된 씬 컴포넌트) 평면 복제
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (!IsValid(Comp) || Visited.count(Comp)) continue;

		UActorComponent* DupComp = Cast<UActorComponent>(Comp->DuplicateWithArchiveContext(Dup, DuplicateContext));
		if (!DupComp) continue;

		DupComp->SetOwner(Dup);
		Dup->RegisterComponent(DupComp);
		Visited.insert(Comp);
	}
	DuplicateContext.ResolveObjectReferenceFixups();

	for (UActorComponent* DupComp : Dup->OwnedComponents)
	{
		if (IsValid(DupComp))
		{
			DupComp->PostDuplicate();
		}
	}

	Dup->bPrimitiveCacheDirty = true;

	// 4) 서브클래스 멤버 포인터 복원
	Dup->PostDuplicate();

	// 5) 월드에 등록 — Dup의 Outer(=대상 World)에 등록해야 PIE 복제 시에도 올바르게 동작.
	if (UWorld* DestWorld = Dup->GetWorld())
	{
		DestWorld->AddActor(Dup);
	}

	return Dup;
}

void AActor::PreGetEditableProperties()
{
	PendingActorLocation = GetActorLocation();
	PendingActorRotation = GetActorRotation();
	PendingActorScale = GetActorScale();
	PendingActorVisible = bVisible;

	// Tags — 콤마 구분 단일 문자열로 편집. PostEditProperty 가 다시 split 해서 Tags 갱신.
	PendingTagsString = JoinTagsCommaSep(Tags);
}

void AActor::PostEditProperty(const char* PropertyName)
{
	UObject::PostEditProperty(PropertyName);
	if (strcmp(PropertyName, "PendingActorLocation") == 0 || strcmp(PropertyName, "Location") == 0)
	{
		SetActorLocation(PendingActorLocation);
	}
	else if (strcmp(PropertyName, "PendingActorRotation") == 0 || strcmp(PropertyName, "Rotation") == 0)
	{
		SetActorRotation(PendingActorRotation);
	}
	else if (strcmp(PropertyName, "PendingActorScale") == 0 || strcmp(PropertyName, "Scale") == 0)
	{
		SetActorScale(PendingActorScale);
	}
	else if (strcmp(PropertyName, "PendingActorVisible") == 0 || strcmp(PropertyName, "Visible") == 0)
	{
		SetVisible(PendingActorVisible);
	}
	else if (strcmp(PropertyName, "PendingTagsString") == 0 || strcmp(PropertyName, "Tags") == 0)
	{
		SetTags(SplitTagsCommaSep(PendingTagsString));
	}
}

const TArray<UPrimitiveComponent*>& AActor::GetPrimitiveComponents() const
{
	if (bPrimitiveCacheDirty)
	{
		PrimitiveCache.clear();
		for (UActorComponent* Comp : OwnedComponents)
		{
            if (IsValid(Comp) && Comp->IsA<UPrimitiveComponent>())
			{
				PrimitiveCache.emplace_back(static_cast<UPrimitiveComponent*>(Comp));
			}
		}
		bPrimitiveCacheDirty = false;
	}
	return PrimitiveCache;
}

void AActor::AddReferencedObjects(FReferenceCollector& Collector)
{
    UObject::AddReferencedObjects(Collector);

    Collector.AddReferencedObject(RootComponent, "AActor.RootComponent");
    Collector.AddReferencedObjects(OwnedComponents, "AActor.OwnedComponents");
}

void AActor::RouteActorDestroyed()
{
    if (bActorDestroyRouted)
    {
        return;
    }

    bActorDestroyRouted = true;

    EndPlay();
    PrimaryActorTick.UnRegisterTickFunction();

    TArray<UActorComponent*> ComponentsToDestroy;
    ComponentsToDestroy.reserve(OwnedComponents.size());
    for (UActorComponent* Component : OwnedComponents)
    {
        if (IsAliveObject(Component))
        {
            ComponentsToDestroy.push_back(Component);
        }
    }

    for (UActorComponent* Component : ComponentsToDestroy)
    {
        if (!IsAliveObject(Component))
        {
            continue;
        }

        Component->EndPlay();
        Component->RouteComponentDestroyed();
        Component->MarkPendingKill();
    }

    OwnedComponents.clear();
    PrimitiveCache.clear();
    RootComponent = nullptr;
    bPrimitiveCacheDirty = true;
    MarkPendingKill();
}

void AActor::BeginDestroy()
{
    if (HasAnyFlags(RF_BeginDestroy))
    {
        return;
    }

    RouteActorDestroyed();
    UObject::BeginDestroy();
}
