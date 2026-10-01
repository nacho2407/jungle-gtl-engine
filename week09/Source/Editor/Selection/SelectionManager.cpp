// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/Scene.h"

void FSelectionManager::Init()
{
    Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
    Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
    Gizmo->Deactivate();
}

void FSelectionManager::SetWorld(UWorld* InWorld)
{
    // 기존 Scene에서 Gizmo 프록시 해제
    if (Gizmo && World)
        Gizmo->DestroyRenderState();

    World = InWorld;

    // 새 Scene에 Gizmo 프록시 등록
    if (Gizmo && World)
    {
        Gizmo->SetScene(&World->GetScene());
        Gizmo->CreateRenderState();
    }

    SyncGizmo();
}

void FSelectionManager::Shutdown()
{
    ClearSelection();

    if (Gizmo)
    {
        UObjectManager::Get().DestroyObject(Gizmo);
        Gizmo = nullptr;
    }
}

void FSelectionManager::Select(AActor* Actor)
{
    if (Actor && IsSelectionBlocked(Actor))
    {
        return;
    }

    if (SelectedActors.size() == 1 && SelectedActors.front() == Actor)
    {
        SyncGizmo();
        return;
    }

    // 기존 선택 해제
    for (AActor* Prev : SelectedActors)
        SetActorProxiesSelected(Prev, false);

    SelectedActors.clear();
    if (Actor)
    {
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
    }
    SyncGizmo();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
    if (!ClickedActor || IsSelectionBlocked(ClickedActor))
        return;

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
    if (ClickedIdx == -1)
        return;

    // Find nearest already-selected actor's index in ActorList
    int32 AnchorIdx = ClickedIdx;
    int32 MinDist = INT_MAX;
    for (AActor* Sel : SelectedActors)
    {
        for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
        {
            if (ActorList[i] == Sel)
            {
                int32 Dist = std::abs(i - ClickedIdx);
                if (Dist < MinDist)
                {
                    MinDist = Dist;
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
    for (AActor* Prev : SelectedActors)
        SetActorProxiesSelected(Prev, false);

    SelectedActors.clear();
    for (int32 i = Lo; i <= Hi; ++i)
    {
        if (ActorList[i])
        {
            if (IsSelectionBlocked(ActorList[i]))
            {
                continue;
            }

            SelectedActors.push_back(ActorList[i]);
            SetActorProxiesSelected(ActorList[i], true);
        }
    }
    SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
    if (!Actor || IsSelectionBlocked(Actor))
        return;

    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
    }
    else
    {
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
    }
    SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        SelectedActors.erase(It);
    }
    SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
    if (SelectedActors.empty())
    {
        return;
    }

    for (AActor* Actor : SelectedActors)
        SetActorProxiesSelected(Actor, false);

    SelectedActors.clear();
    SyncGizmo();
}

void FSelectionManager::Tick()
{
    if (!Gizmo || !bGizmoEnabled)
    {
        return;
    }

    AActor* Primary = GetPrimarySelection();
    if (!Primary)
    {
        return;
    }

    if (Gizmo->GetTarget() != Primary)
    {
        SyncGizmo();
        return;
    }

    Gizmo->UpdateGizmoTransform();
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

bool FSelectionManager::IsSelectionBlocked(AActor* Actor) const
{
    return Actor && std::find(SelectionBlockedActors.begin(), SelectionBlockedActors.end(), Actor) != SelectionBlockedActors.end();
}

void FSelectionManager::AddSelectionBlock(AActor* Actor)
{
    if (!Actor || IsSelectionBlocked(Actor))
    {
        return;
    }

    SelectionBlockedActors.push_back(Actor);
    Deselect(Actor);
}

void FSelectionManager::RemoveSelectionBlock(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    auto It = std::find(SelectionBlockedActors.begin(), SelectionBlockedActors.end(), Actor);
    if (It != SelectionBlockedActors.end())
    {
        SelectionBlockedActors.erase(It);
    }
}

void FSelectionManager::SetActorProxiesSelected(AActor* Actor, bool bSelected)
{
    if (!Actor || !World)
        return;

    FScene& Scene = World->GetScene();
    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (FPrimitiveProxy* Proxy = Prim->GetSceneProxy())
        {
            Scene.SetProxySelected(Proxy, bSelected);
        }
    }
}

void FSelectionManager::SyncGizmo()
{
    if (!Gizmo)
        return;

    if (!bGizmoEnabled)
    {
        Gizmo->Deactivate();
        return;
    }

    AActor* Primary = GetPrimarySelection();
    if (Primary)
    {
        Gizmo->SetTarget(Primary);
        Gizmo->SetSelectedActors(&SelectedActors);
    }
    else
    {
        Gizmo->SetSelectedActors(nullptr);
        Gizmo->Deactivate();
    }
}
