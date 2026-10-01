// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class AActor;
class UGizmoComponent;
class UWorld;

// FSelectionManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FSelectionManager
{
public:
    void Init();
    void Shutdown();

    void Select(AActor* Actor);
    void SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList);
    void ToggleSelect(AActor* Actor);
    void Deselect(AActor* Actor);
    void ClearSelection();
    void Tick();

    bool IsSelected(AActor* Actor) const
    {
        return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
    }

    AActor* GetPrimarySelection() const
    {
        return SelectedActors.empty() ? nullptr : SelectedActors.front();
    }

    const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }
    bool IsEmpty() const { return SelectedActors.empty(); }
    bool IsSelectionBlocked(AActor* Actor) const;

    UGizmoComponent* GetGizmo() const { return Gizmo; }

    void SetGizmoEnabled(bool bEnabled);
    void SetWorld(UWorld* InWorld);
    void AddSelectionBlock(AActor* Actor);
    void RemoveSelectionBlock(AActor* Actor);

private:
    void SyncGizmo();
    void SetActorProxiesSelected(AActor* Actor, bool bSelected);

    TArray<AActor*> SelectedActors;
    TArray<AActor*> SelectionBlockedActors;
    UGizmoComponent* Gizmo = nullptr;
    UWorld* World = nullptr;
    bool bGizmoEnabled = true;
};
