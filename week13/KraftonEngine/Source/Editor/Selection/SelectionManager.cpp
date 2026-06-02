#include "Editor/Selection/SelectionManager.h"

#include "Component/ActorComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Render/Scene/FScene.h"

#include <algorithm>

FSelectionDetailTarget FSelectionDetailTarget::FromObject(UObject* Object)
{
    FSelectionDetailTarget Target;
    if (IsValid(Object))
    {
        Target.ObjectPtr = Object;
        Target.StructType = Object->GetClass();
        Target.ContainerPtr = Object;
    }
    return Target;
}

void FSelectionDetailTarget::Reset()
{
    ObjectPtr = nullptr;
    StructType = nullptr;
    ContainerPtr = nullptr;
}

bool FSelectionDetailTarget::HasTarget() const
{
    return StructType != nullptr && ContainerPtr != nullptr;
}

bool FSelectionDetailTarget::IsValidTarget() const
{
    if (!HasTarget())
    {
        return false;
    }

    return ObjectPtr == nullptr || IsValid(ObjectPtr);
}

namespace
{
    AActor* GetActorFromTarget(const FSelectionDetailTarget& Target)
    {
        if (!Target.IsValidTarget())
        {
            return nullptr;
        }

        if (AActor* Actor = Cast<AActor>(Target.ObjectPtr))
        {
            return Actor;
        }

        if (UActorComponent* Component = Cast<UActorComponent>(Target.ObjectPtr))
        {
            return Component->GetOwner();
        }

        return nullptr;
    }

    UActorComponent* GetComponentFromTarget(const FSelectionDetailTarget& Target)
    {
        return Target.IsValidTarget() ? Cast<UActorComponent>(Target.ObjectPtr) : nullptr;
    }

    bool ContainsActor(const TArray<AActor*>& Actors, AActor* Actor)
    {
        return std::find(Actors.begin(), Actors.end(), Actor) != Actors.end();
    }
}

USceneComponent* FSelectionManager::GetSelectedComponent() const
{
    return IsValid(SelectedComponent) ? SelectedComponent : nullptr;
}

UActorComponent* FSelectionManager::GetSelectedActorComponent() const
{
    const FSelectionDetailTarget* PrimaryTarget = GetPrimaryDetailTarget();
    return PrimaryTarget ? GetComponentFromTarget(*PrimaryTarget) : nullptr;
}

bool FSelectionManager::IsComponentDetailsSelected() const
{
    return GetSelectedActorComponent() != nullptr;
}

const FSelectionDetailTarget* FSelectionManager::GetPrimaryDetailTarget() const
{
    for (const FSelectionDetailTarget& Target : SelectedDetailTargets)
    {
        if (Target.IsValidTarget())
        {
            return &Target;
        }
    }

    return nullptr;
}

bool FSelectionManager::IsSelected(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    for (const FSelectionDetailTarget& Target : SelectedDetailTargets)
    {
        if (GetActorFromTarget(Target) == Actor)
        {
            return true;
        }
    }

    return false;
}

AActor* FSelectionManager::GetPrimarySelection() const
{
    const FSelectionDetailTarget* PrimaryTarget = GetPrimaryDetailTarget();
    return PrimaryTarget ? GetActorFromTarget(*PrimaryTarget) : nullptr;
}

UGizmoComponent* FSelectionManager::GetGizmo() const
{
    return IsValid(Gizmo) ? Gizmo : nullptr;
}

TArray<AActor*> FSelectionManager::GetSelectedActors() const
{
    TArray<AActor*> Actors;
    for (const FSelectionDetailTarget& Target : SelectedDetailTargets)
    {
        AActor* Actor = GetActorFromTarget(Target);
        if (IsValid(Actor) && !ContainsActor(Actors, Actor))
        {
            Actors.push_back(Actor);
        }
    }
    return Actors;
}

void FSelectionManager::Init()
{
    Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
    if (!Gizmo)
    {
        return;
    }

    Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
    Gizmo->Deactivate();
}

void FSelectionManager::Shutdown()
{
    ClearSelection();
    World = nullptr;

    if (Gizmo)
    {
        UObjectManager::Get().DestroyObject(Gizmo);
        Gizmo = nullptr;
    }
}

void FSelectionManager::Select(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor))
    {
        ClearSelection();
        return;
    }

    const FSelectionDetailTarget* PrimaryTarget = GetPrimaryDetailTarget();
    USceneComponent* RootComponent = Actor->GetRootComponent();
    if (SelectedDetailTargets.size() == 1 && PrimaryTarget && PrimaryTarget->ObjectPtr == Actor && SelectedComponent == RootComponent)
    {
        return;
    }

    SetSingleDetailTarget(FSelectionDetailTarget::FromObject(Actor));
    SetActorProxiesSelected(Actor, true);
    SelectedComponent = RootComponent;

    SyncGizmo();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
    PruneInvalidSelection();

    if (!IsValid(ClickedActor)) return;

    int32 ClickedIdx = -1;
    for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
    {
        if (ActorList[i] == ClickedActor)
        {
            ClickedIdx = i;
            break;
        }
    }
    if (ClickedIdx == -1) return;

    int32 MinDist = INT_MAX;
    int32 AnchorIdx = ClickedIdx;
    for (AActor* Sel : GetSelectedActors())
    {
        if (!IsValid(Sel))
        {
            continue;
        }

        for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
        {
            if (ActorList[i] == Sel)
            {
                const int32 Dist = std::abs(i - ClickedIdx);
                if (Dist < MinDist)
                {
                    MinDist = Dist;
                    AnchorIdx = i;
                }
                break;
            }
        }
    }

    for (AActor* Prev : GetSelectedActors())
    {
        SetActorProxiesSelected(Prev, false);
    }

    SelectedDetailTargets.clear();
    SelectedComponent = nullptr;

    const int32 Lo = std::min(AnchorIdx, ClickedIdx);
    const int32 Hi = std::max(AnchorIdx, ClickedIdx);
    for (int32 i = Lo; i <= Hi; ++i)
    {
        AddActorDetailTarget(ActorList[i]);
    }

    RefreshDerivedSelection();
    SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor)) return;

    TArray<AActor*> PreviousActors = GetSelectedActors();
    auto It = std::find_if(
        SelectedDetailTargets.begin(),
        SelectedDetailTargets.end(),
        [Actor](const FSelectionDetailTarget& Target)
        {
            return Target.ObjectPtr == Actor;
        });

    if (It != SelectedDetailTargets.end())
    {
        SelectedDetailTargets.erase(It);
        SetActorProxiesSelected(Actor, false);
        if (SelectedComponent && IsAliveObject(SelectedComponent) && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = nullptr;
        }
    }
    else
    {
        AddActorDetailTarget(Actor);
        SetActorProxiesSelected(Actor, true);
    }

    RefreshDerivedSelection();
    SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor)) return;

    const size_t OldCount = SelectedDetailTargets.size();
    SelectedDetailTargets.erase(
        std::remove_if(
            SelectedDetailTargets.begin(),
            SelectedDetailTargets.end(),
            [Actor](const FSelectionDetailTarget& Target)
            {
                return GetActorFromTarget(Target) == Actor;
            }),
        SelectedDetailTargets.end()
    );

    if (OldCount != SelectedDetailTargets.size())
    {
        SetActorProxiesSelected(Actor, false);
        if (SelectedComponent && IsAliveObject(SelectedComponent) && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = nullptr;
        }
        RefreshDerivedSelection();
    }

    SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
    PruneInvalidSelection();

    if (SelectedDetailTargets.empty() && SelectedComponent == nullptr)
    {
        return;
    }

    for (AActor* Actor : GetSelectedActors())
    {
        SetActorProxiesSelected(Actor, false);
    }

    SelectedDetailTargets.clear();
    SelectedComponent = nullptr;
    GizmoSelectedActors.clear();
    SyncGizmo();
}

int32 FSelectionManager::DeleteSelectedActors()
{
    PruneInvalidSelection();

    TArray<AActor*> ActorsToDelete = GetSelectedActors();
    if (!IsValid(World) || ActorsToDelete.empty())
    {
        return 0;
    }

    const int32 DeletedCount = static_cast<int32>(ActorsToDelete.size());
    ClearSelection();

    World->BeginDeferredPickingBVHUpdate();
    for (AActor* Actor : ActorsToDelete)
    {
        if (IsValid(Actor))
        {
            World->DestroyActor(Actor);
        }
    }
    World->EndDeferredPickingBVHUpdate();

    return DeletedCount;
}

void FSelectionManager::Tick()
{
    PruneInvalidSelection();

    if (!IsValid(Gizmo) || !bGizmoEnabled)
    {
        return;
    }

    USceneComponent* Primary = SelectedComponent;
    if (!IsValid(Primary))
    {
        return;
    }

    if (Gizmo->GetTargetComponent() != Primary)
    {
        SyncGizmo();
        return;
    }

    Gizmo->UpdateGizmoTransform();
}

void FSelectionManager::SelectComponent(USceneComponent* Component)
{
    PruneInvalidSelection();

    if (!IsValid(Component))
    {
        return;
    }

    USceneComponent* Target = Component;
    if (Component->IsEditorOnlyComponent())
    {
        if (IsValid(Component->GetParent()))
        {
            Target = Component->GetParent();
        }
        else if (AActor* ComponentOwner = Component->GetOwner(); IsValid(ComponentOwner))
        {
            Target = ComponentOwner->GetRootComponent();
        }
    }

    if (!IsValid(Target))
    {
        return;
    }

    AActor* Owner = Target->GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    SetSingleDetailTarget(FSelectionDetailTarget::FromObject(Target));
    SetActorProxiesSelected(Owner, true);
    SelectedComponent = Target;

    SyncGizmo();
}

void FSelectionManager::SelectActorDetails(AActor* Actor)
{
    Select(Actor);
}

void FSelectionManager::SelectActorComponent(UActorComponent* Component)
{
    PruneInvalidSelection();

    if (!IsValid(Component))
    {
        return;
    }

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
    {
        SelectComponent(SceneComponent);
        return;
    }

    AActor* Owner = Component->GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    SetSingleDetailTarget(FSelectionDetailTarget::FromObject(Component));
    SetActorProxiesSelected(Owner, true);
    SelectedComponent = nullptr;

    SyncGizmo();
}

void FSelectionManager::SetGizmoEnabled(bool bEnabled)
{
    if (bGizmoEnabled == bEnabled)
    {
        return;
    }

    bGizmoEnabled = bEnabled;
    SyncGizmo();
}

void FSelectionManager::SetWorld(UWorld* InWorld)
{
    PruneInvalidSelection();

    if (Gizmo && IsValid(World))
        Gizmo->DestroyRenderState();

    World = IsValid(InWorld) ? InWorld : nullptr;

    if (IsValid(Gizmo) && IsValid(World))
    {
        Gizmo->SetScene(&World->GetScene());
        Gizmo->CreateRenderState();
    }

    SyncGizmo();
}

void FSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
    Collector.AddReferencedObject(Gizmo);
}

void FSelectionManager::PruneInvalidSelection()
{
    TArray<AActor*> PreviousActors = GetSelectedActors();

    SelectedDetailTargets.erase(
        std::remove_if(
            SelectedDetailTargets.begin(),
            SelectedDetailTargets.end(),
            [](const FSelectionDetailTarget& Target)
            {
                return !Target.IsValidTarget();
            }),
        SelectedDetailTargets.end()
    );

    for (AActor* Actor : PreviousActors)
    {
        if (IsValid(Actor) && !IsSelected(Actor))
        {
            SetActorProxiesSelected(Actor, false);
        }
    }

    if (SelectedComponent)
    {
        AActor* Owner = IsAliveObject(SelectedComponent) ? SelectedComponent->GetOwner() : nullptr;
        if (!IsValid(SelectedComponent) || !IsValid(Owner) || !IsSelected(Owner))
        {
            SelectedComponent = nullptr;
        }
    }

    RefreshDerivedSelection();
}

void FSelectionManager::SyncGizmo()
{
    PruneInvalidSelection();

    if (!IsValid(Gizmo)) return;

    if (!bGizmoEnabled)
    {
        Gizmo->Deactivate();
        return;
    }

    RefreshGizmoSelectedActors();
    if (IsValid(SelectedComponent))
    {
        Gizmo->SetSelectedActors(GizmoSelectedActors.empty() ? nullptr : &GizmoSelectedActors);
        Gizmo->SetTarget(SelectedComponent);
    }
    else
    {
        Gizmo->SetSelectedActors(nullptr);
        Gizmo->Deactivate();
    }
}

void FSelectionManager::SetSingleDetailTarget(const FSelectionDetailTarget& Target)
{
    for (AActor* Actor : GetSelectedActors())
    {
        SetActorProxiesSelected(Actor, false);
    }

    SelectedDetailTargets.clear();
    if (Target.IsValidTarget())
    {
        SelectedDetailTargets.push_back(Target);
    }
}

void FSelectionManager::AddActorDetailTarget(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    const bool bAlreadySelected = std::any_of(
        SelectedDetailTargets.begin(),
        SelectedDetailTargets.end(),
        [Actor](const FSelectionDetailTarget& Target)
        {
            return Target.ObjectPtr == Actor;
        });

    if (!bAlreadySelected)
    {
        SelectedDetailTargets.push_back(FSelectionDetailTarget::FromObject(Actor));
        SetActorProxiesSelected(Actor, true);
    }
}

void FSelectionManager::RefreshDerivedSelection()
{
    const FSelectionDetailTarget* PrimaryTarget = GetPrimaryDetailTarget();
    if (!PrimaryTarget)
    {
        SelectedComponent = nullptr;
        return;
    }

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(PrimaryTarget->ObjectPtr))
    {
        SelectedComponent = SceneComponent;
        return;
    }

    if (Cast<UActorComponent>(PrimaryTarget->ObjectPtr))
    {
        SelectedComponent = nullptr;
        return;
    }

    if (!SelectedComponent)
    {
        if (AActor* Actor = Cast<AActor>(PrimaryTarget->ObjectPtr))
        {
            SelectedComponent = Actor->GetRootComponent();
        }
    }
}

void FSelectionManager::RefreshGizmoSelectedActors()
{
    GizmoSelectedActors = GetSelectedActors();
}

void FSelectionManager::SetActorProxiesSelected(AActor* Actor, bool bSelected)
{
    if (!IsValid(Actor) || !IsValid(World)) return;

    FScene& Scene = World->GetScene();
    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!IsValid(Prim))
        {
            continue;
        }

        if (FPrimitiveSceneProxy* Proxy = Prim->GetSceneProxy())
        {
            if (Proxy->HasValidOwner())
            {
                Scene.SetProxySelected(Proxy, bSelected);
            }
        }
    }
}
