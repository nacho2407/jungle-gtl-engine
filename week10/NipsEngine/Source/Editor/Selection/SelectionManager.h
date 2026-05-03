#pragma once

#include "Core/CoreMinimal.h"

class AActor;
class UActorComponent;
class UGizmoComponent;

class FSelectionManager
{
public:
	void Init();
	void Shutdown();

	void Select(AActor* Actor);
	void AddSelect(AActor* Actor);
	void SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList);
	void ToggleSelect(AActor* Actor);
	void Deselect(AActor* Actor);
	void ClearSelection();
	void SelectComponent(UActorComponent* Component);
	void ClearComponentSelection();
	void BeginBatchUpdate();
	void EndBatchUpdate();

	bool IsSelected(AActor* Actor) const
	{
		return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
	}

	AActor* GetPrimarySelection() const
	{
		return SelectedActors.empty() ? nullptr : SelectedActors.back();
	}

	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }
	const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }
	bool IsEmpty() const { return SelectedActors.empty(); }

	UGizmoComponent* GetGizmo() const { return Gizmo; }

	void OnActorDestroyed(AActor* Actor);

private:
	void RequestGizmoSync();
	void SyncGizmo();

	TArray<AActor*> SelectedActors;
	UActorComponent* SelectedComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	int32 BatchUpdateDepth = 0;
	bool bPendingGizmoSync = false;
};
