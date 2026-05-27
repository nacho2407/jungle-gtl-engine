#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/FScene.h"
#include "Object/GarbageCollection.h"

#include <algorithm>

USceneComponent* FSelectionManager::GetSelectedComponent() const
{
    return IsValid(SelectedComponent) ? SelectedComponent : nullptr;
}

bool FSelectionManager::IsSelected(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    return std::find_if(
        SelectedActors.begin(),
        SelectedActors.end(),
        [Actor](AActor* SelectedActor)
        {
            return IsValid(SelectedActor) && SelectedActor == Actor;
        }) != SelectedActors.end();
}

AActor* FSelectionManager::GetPrimarySelection() const
{
    for (AActor* Actor : SelectedActors)
    {
        if (IsValid(Actor))
        {
            return Actor;
        }
    }

    return nullptr;
}

UGizmoComponent* FSelectionManager::GetGizmo() const
{
    return IsValid(Gizmo) ? Gizmo : nullptr;
}

TArray<AActor*> FSelectionManager::GetSelectedActors() const
{
    TArray<AActor*> ValidActors;
    ValidActors.reserve(SelectedActors.size());

    for (AActor* Actor : SelectedActors)
    {
        if (IsValid(Actor))
        {
            ValidActors.push_back(Actor);
        }
    }

    return ValidActors;
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

    USceneComponent* RootComponent = Actor->GetRootComponent();
    if (!IsValid(RootComponent))
    {
        ClearSelection();
        return;
    }

    if (SelectedActors.size() == 1 && SelectedActors.front() == Actor && SelectedComponent == RootComponent)
    {
        return;
    }

    // 기존 선택 해제
    for (AActor* Prev : SelectedActors)
    {
        SetActorProxiesSelected(Prev, false);
    }

    SelectedActors.clear();
    SelectedActors.push_back(Actor);
    SetActorProxiesSelected(Actor, true);
    SelectedComponent = RootComponent;

    SyncGizmo();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
    PruneInvalidSelection();

    if (!IsValid(ClickedActor)) return;

    // Find index of clicked actor
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

    // Find nearest already-selected actor's index in ActorList
    int32 AnchorIdx = ClickedIdx;
    int32 MinDist   = INT_MAX;
    for (AActor* Sel : SelectedActors)
    {
        if (!IsValid(Sel))
        {
            continue;
        }

        for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
        {
            if (ActorList[i] == Sel)
            {
                int32 Dist = std::abs(i - ClickedIdx);
                if (Dist < MinDist)
                {
                    MinDist   = Dist;
                    AnchorIdx = i;
                }
                break;
            }
        }
    }

    // Replace selection with range [min, max]
    int32 Lo = std::min(AnchorIdx, ClickedIdx);
    int32 Hi = std::max(AnchorIdx, ClickedIdx);

    // 기존 선택 해제
    for (AActor* Prev : SelectedActors) SetActorProxiesSelected(Prev, false);

    SelectedActors.clear();
    SelectedComponent = nullptr;

    for (int32 i = Lo; i <= Hi; ++i)
    {
        AActor* Actor = ActorList[i];
        if (IsValid(Actor))
        {
            SelectedActors.push_back(Actor);
            SetActorProxiesSelected(Actor, true);
        }
    }

    PruneInvalidSelection();
    SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
    PruneInvalidSelection();

    if (!IsValid(Actor)) return;

    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
        if (SelectedComponent && IsAliveObject(SelectedComponent) && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = nullptr;
            PruneInvalidSelection();
        }
    }
    else
    {
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
        if (SelectedActors.size() == 1)
        {
            USceneComponent* RootComponent = Actor->GetRootComponent();
            SelectedComponent              = IsValid(RootComponent) ? RootComponent : nullptr;
        }
    }
    SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
    PruneInvalidSelection();

    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
        if (SelectedComponent && IsAliveObject(SelectedComponent) && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = nullptr;
            PruneInvalidSelection();
        }
    }
    SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
    PruneInvalidSelection();

    if (SelectedActors.empty() && SelectedComponent == nullptr)
    {
        return;
    }

    for (AActor* Actor : SelectedActors)
    {
        SetActorProxiesSelected(Actor, false);
    }

    SelectedActors.clear();
    SelectedComponent = nullptr;
    SyncGizmo();
}

int32 FSelectionManager::DeleteSelectedActors()
{
    PruneInvalidSelection();

    if (!IsValid(World) || SelectedActors.empty())
    {
        return 0;
    }

    TArray<AActor*> ActorsToDelete = SelectedActors;
    const int32     DeletedCount   = static_cast<int32>(ActorsToDelete.size());

    // 파괴 전에 선택/기즈모 참조를 먼저 끊어 dangling target을 방지한다.
    ClearSelection();

    World->BeginDeferredPickingBVHUpdate();
    for (AActor* Actor : ActorsToDelete)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        World->DestroyActor(Actor);
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

    // [버그 수정] 에디터 전용 컴포넌트(광원 아이콘 등)는 개별 조작 대상이 아니므로,
    // 부모 컴포넌트로 리다이렉트하여 함께 움직이도록 합니다.
    USceneComponent* Target = Component;
    if (Component->IsEditorOnlyComponent())
    {
        if (IsValid(Component->GetParent()))
        {
            Target = Component->GetParent();
        }
        else
        {
            AActor* ComponentOwner = Component->GetOwner();
            if (IsValid(ComponentOwner))
            {
                Target = ComponentOwner->GetRootComponent();
            }
        }
    }

    if (!IsValid(Target))
    {
        return;
    }

    if (SelectedComponent == Target)
    {
        return;
    }

    AActor* Owner = Target->GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    if (!IsSelected(Owner))
    {
        Select(Owner);
    }

    // Select(Owner)는 actor root를 선택 대상으로 잡기 때문에, owner 선택 보장 후
    // 실제 component 선택 대상을 다시 설정합니다.
    SelectedComponent = Target;

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

    // 기존 Scene에서 Gizmo 프록시 해제
    if (Gizmo && IsValid(World))
        Gizmo->DestroyRenderState();

    World = IsValid(InWorld) ? InWorld : nullptr;

    // 새 Scene에 Gizmo 프록시 등록
    if (IsValid(Gizmo) && IsValid(World))
    {
        Gizmo->SetScene(&World->GetScene());
        Gizmo->CreateRenderState();
    }

    SyncGizmo();
}

void FSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
    // Selection targets/world are weak references. The editor-owned gizmo is the only UObject
    // whose lifetime is owned by the selection manager.
    Collector.AddReferencedObject(Gizmo);
}

void FSelectionManager::PruneInvalidSelection()
{
    bool bSelectionChanged = false;

    const size_t OldActorCount = SelectedActors.size();
    SelectedActors.erase(
        std::remove_if(
            SelectedActors.begin(),
            SelectedActors.end(),
            [](AActor* Actor)
            {
                return !IsValid(Actor);
            }
        ),
        SelectedActors.end()
    );
    bSelectionChanged = bSelectionChanged || OldActorCount != SelectedActors.size();

    if (SelectedComponent)
    {
        AActor* Owner = IsAliveObject(SelectedComponent) ? SelectedComponent->GetOwner() : nullptr;
        if (!IsValid(SelectedComponent) || !IsValid(Owner))
        {
            SelectedComponent = nullptr;
            bSelectionChanged = true;
        }
    }

    if (SelectedComponent)
    {
        AActor* Owner = SelectedComponent->GetOwner();
        if (!IsValid(Owner) || !IsSelected(Owner))
        {
            SelectedComponent = nullptr;
            bSelectionChanged = true;
        }
    }

    if (!SelectedComponent && !SelectedActors.empty())
    {
        for (AActor* Actor : SelectedActors)
        {
            if (!IsValid(Actor))
            {
                continue;
            }

            USceneComponent* Root = Actor->GetRootComponent();
            if (IsValid(Root))
            {
                SelectedComponent = Root;
                break;
            }
        }
    }

    if (bSelectionChanged && IsValid(Gizmo))
    {
        Gizmo->SetSelectedActors(SelectedActors.empty() ? nullptr : &SelectedActors);
    }
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

    USceneComponent* Primary = SelectedComponent;
    if (IsValid(Primary))
    {
        Gizmo->SetSelectedActors(SelectedActors.empty() ? nullptr : &SelectedActors);
        Gizmo->SetTarget(Primary);
    }
    else
    {
        Gizmo->SetSelectedActors(nullptr);
        Gizmo->Deactivate();
    }
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

