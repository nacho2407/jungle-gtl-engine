#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

#include <algorithm>

class AActor;
class USceneComponent;
class UGizmoComponent;
class UWorld;

class FSelectionManager : public FGCObject
{
public:
	void Init();
	void Shutdown();

	void Select(AActor* Actor);
	void SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList);
	void ToggleSelect(AActor* Actor);
	void Deselect(AActor* Actor);
	void ClearSelection();
	int32 DeleteSelectedActors();
	void Tick();

	void SelectComponent(USceneComponent* Component);
	USceneComponent* GetSelectedComponent() const;

	bool IsSelected(AActor* Actor) const;

	AActor* GetPrimarySelection() const;

	TArray<AActor*> GetSelectedActors() const;
	bool IsEmpty() const { return GetSelectedActors().empty() && GetSelectedComponent() == nullptr; }

	UGizmoComponent* GetGizmo() const;

	void SetGizmoEnabled(bool bEnabled);
	void SetWorld(UWorld* InWorld);
	const char* GetReferencerName() const override { return "FSelectionManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void PruneInvalidSelection();
	void SyncGizmo();
	void SetActorProxiesSelected(AActor* Actor, bool bSelected);

	TArray<AActor*> SelectedActors;
	USceneComponent* SelectedComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	UWorld* World = nullptr;
	bool bGizmoEnabled = true;
};
