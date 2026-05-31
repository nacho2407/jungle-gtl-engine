#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

class AActor;
class UActorComponent;
class UObject;
class USceneComponent;
class UStruct;
class UGizmoComponent;
class UWorld;

struct FSelectionDetailTarget
{
	UObject* ObjectPtr = nullptr;
	UStruct* StructType = nullptr;
	void* ContainerPtr = nullptr;

	static FSelectionDetailTarget FromObject(UObject* Object);

	void Reset();
	bool HasTarget() const;
	bool IsValidTarget() const;
};

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
	void SelectActorDetails(AActor* Actor);
	void SelectActorComponent(UActorComponent* Component);
	UActorComponent* GetSelectedActorComponent() const;
	bool IsComponentDetailsSelected() const;
	const FSelectionDetailTarget* GetPrimaryDetailTarget() const;
	const TArray<FSelectionDetailTarget>& GetSelectedDetailTargets() const { return SelectedDetailTargets; }

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
	void SetSingleDetailTarget(const FSelectionDetailTarget& Target);
	void AddActorDetailTarget(AActor* Actor);
	void RefreshDerivedSelection();
	void RefreshGizmoSelectedActors();
	void SetActorProxiesSelected(AActor* Actor, bool bSelected);

	TArray<FSelectionDetailTarget> SelectedDetailTargets;
	TArray<AActor*> GizmoSelectedActors;
	USceneComponent* SelectedComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	UWorld* World = nullptr;
	bool bGizmoEnabled = true;
};
