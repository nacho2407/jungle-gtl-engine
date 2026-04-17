#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "GameFramework/World.h"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor()
{
    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
        OwningWorld = nullptr;
    }

    for (auto* Comp : OwnedComponents)
    {
        UObjectManager::Get().DestroyObject(Comp);
    }

    OwnedComponents.clear();
    RootComponent = nullptr;
}

/* 액터의 계층 구조를 복원하기 위해 자식들을 재귀적으로 순회합니다. */
static USceneComponent* DuplicateSubTree(
    USceneComponent* OriginalComp, AActor* NewActor, TArray<UActorComponent*>& OwnedComponents,
    TMap<USceneComponent*, USceneComponent*>& OutCompMap)
{
    if (!OriginalComp)
        return nullptr;

    // 현재 노드(부모) 복제
    USceneComponent* DuplicatedComp = Cast<USceneComponent>(OriginalComp->Duplicate());
    if (!DuplicatedComp)
        return nullptr; // 에디터 전용 등이면 nullptr 반환됨

    // 소유자 등록
    DuplicatedComp->SetOwner(NewActor);
    OwnedComponents.push_back(DuplicatedComp);

    // 원본 → 복제본 매핑 등록 (MovementComponent의 UpdatedComponent 연결에 사용)
    OutCompMap[OriginalComp] = DuplicatedComp;

    // 자식들을 재귀적으로 순회하며 방금 복제된 자신(DuplicatedComp)에게 바로 Attach
    for (USceneComponent* Child : OriginalComp->GetChildren())
    {
        USceneComponent* DuplicatedChild = DuplicateSubTree(Child, NewActor, OwnedComponents, OutCompMap);
        if (DuplicatedChild)
        {
            DuplicatedChild->AttachToComponent(DuplicatedComp);
        }
    }

    return DuplicatedComp;
}

void AActor::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
    OutProps.push_back({ "Tick In Editor", EPropertyType::Bool, &bTickInEditor });
    OutProps.push_back({ "Pending Location", EPropertyType::Vec3, &PendingActorLocation });
}

void AActor::PostDuplicate(UObject* Original)
{
    AActor* OrigActor = static_cast<AActor*>(Original);

    OwnedComponents.clear();

    // MovementComponent 등 일반 컴포넌트들의 참조를 복원하기 위한 맵을 선언합니다.
    TMap<USceneComponent*, USceneComponent*> ComponentMap;

    // 1. RootComponent부터 씬 컴포넌트 트리 전체를 복제하여 조립합니다.
    if (OrigActor->RootComponent)
    {
        SetRootComponent(DuplicateSubTree(OrigActor->RootComponent, this, OwnedComponents, ComponentMap));
    }

    // 2. SceneComponent가 아닌 일반 컴포넌트들을 복제하기 위해 배열을 순회합니다.
    for (UActorComponent* OriginalComp : OrigActor->OwnedComponents)
    {
        if (!OriginalComp)
            continue;

        // 씬 컴포넌트는 처리했으므로 건너뜁니다.
        if (OriginalComp->IsA<USceneComponent>())
            continue;

        UActorComponent* DuplicatedComp = Cast<UActorComponent>(OriginalComp->Duplicate());
        if (!DuplicatedComp)
            continue;

        DuplicatedComp->SetOwner(this);
        OwnedComponents.push_back(DuplicatedComp);

        // MovementComponent도 기존에 제어하던 컴포넌트를 제어하도록 연결해줍니다.
        if (UMovementComponent* OriginalMoveComp = Cast<UMovementComponent>(OriginalComp))
        {
            UMovementComponent* DuplicatedMoveComp = Cast<UMovementComponent>(DuplicatedComp);
            USceneComponent* OldTarget = OriginalMoveComp->GetUpdatedComponent();

            if (OldTarget && ComponentMap.find(OldTarget) != ComponentMap.end())
            {
                DuplicatedMoveComp->SetUpdatedComponent(ComponentMap[OldTarget]);
            }
            else
            {
                DuplicatedMoveComp->SetUpdatedComponent(GetRootComponent());
            }
        }
    }

    bPrimitiveCacheDirty = true;
}

void AActor::Serialize(FArchive& Ar)
{
	Ar.BeginObject(std::to_string(GetUUID()));
	UObject::Serialize(Ar);
	Ar << "Visible" << bVisible;
	Ar << "Editor Only" << bTickInEditor;
	Ar.EndObject();

	for (UActorComponent* Comp : OwnedComponents)
	{
		Ar.BeginObject(std::to_string(Comp->GetUUID()));
		Comp->Serialize(Ar);
		Ar.EndObject();
	}
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class)
{
    if (!Class)
        return nullptr;

    UObject* Obj = FObjectFactory::Get().Create(Class->name);
    if (!Obj)
        return nullptr;

    UActorComponent* Comp = Cast<UActorComponent>(Obj);
    if (!Comp)
    {
        UObjectManager::Get().DestroyObject(Obj);
        return nullptr;
    }

    Comp->SetOwner(this);
    OwnedComponents.push_back(Comp);
    bPrimitiveCacheDirty = true;
    NotifyComponentRegistered(Comp);
    return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp)
{
    if (!Comp)
        return;

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
    if (it == OwnedComponents.end())
    {
        Comp->SetOwner(this);
        OwnedComponents.push_back(Comp);
        bPrimitiveCacheDirty = true;
        NotifyComponentRegistered(Comp);
    }
}

void AActor::RemoveComponent(UActorComponent* Component)
{
    if (!Component)
        return;

    NotifyComponentUnregistered(Component);

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
    if (it != OwnedComponents.end())
    {
        OwnedComponents.erase(it);
        bPrimitiveCacheDirty = true;
    }

    // RootComponent가 제거되면 nullptr로
    if (RootComponent == Component)
        RootComponent = nullptr;

    UObjectManager::Get().DestroyObject(Component);
}

void AActor::SetVisible(bool Visible)
{
    if (bVisible == Visible)
    {
        return;
    }

    bVisible = Visible;
    MarkPrimitiveComponentsDirty();
}

void AActor::SetWorld(UWorld* World)
{
    if (OwningWorld == World)
    {
        return;
    }

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
    }

    OwningWorld = World;

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().RegisterActor(this);
    }
}

void AActor::SetRootComponent(USceneComponent* Comp)
{
    if (!Comp)
        return;
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

void AActor::BeginPlay()
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->BeginPlay();
        }
    }
}

void AActor::Tick(float DeltaTime)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component && Component->IsActive())
        {
            Component->ExecuteTick(DeltaTime);
        }
    }
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->EndPlay();
        }
    }
}

void AActor::NotifyComponentRegistered(UActorComponent* Component)
{
    if (Component == nullptr || OwningWorld == nullptr)
    {
        return;
    }

    UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
    if (Primitive == nullptr)
    {
        return;
    }

    OwningWorld->GetSpatialIndex().RegisterPrimitive(Primitive);
    OwningWorld->GetSpatialIndex().FlushDirtyBounds();
}

void AActor::NotifyComponentUnregistered(UActorComponent* Component)
{
    if (Component == nullptr || OwningWorld == nullptr)
    {
        return;
    }

    UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
    if (Primitive == nullptr)
    {
        return;
    }

    OwningWorld->GetSpatialIndex().UnregisterPrimitive(Primitive);
}

void AActor::MarkPrimitiveComponentsDirty()
{
    if (OwningWorld == nullptr)
    {
        return;
    }

    for (UPrimitiveComponent* Primitive : GetPrimitiveComponents())
    {
        OwningWorld->GetSpatialIndex().MarkPrimitiveDirty(Primitive);
    }
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
