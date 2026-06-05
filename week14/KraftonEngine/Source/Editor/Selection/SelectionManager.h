#pragma once

#include "Core/Types/CoreTypes.h"

class AActor;
class USceneComponent;
class UGizmoComponent;
class UWorld;
class FReferenceCollector;

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
	int32 DeleteSelectedActors();
	void Tick();

	void SelectComponent(USceneComponent* Component);
	USceneComponent* GetSelectedComponent() const { return SelectedComponent; }

	bool IsSelected(AActor* Actor) const
	{
		return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
	}

	AActor* GetPrimarySelection() const
	{
		return SelectedActors.empty() ? nullptr : SelectedActors.front();
	}

	const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }

	/**
	 * @brief 선택된 actor들의 UUID 목록 반환
	 *
	 * @return 선택된 actor들의 UUID 목록
	 */
	TArray<uint32> GetSelectedActorUUIDs() const;

	/**
	 * @brief 선택된 component UUID 반환
	 *
	 * @return 선택된 component UUID. 선택된 component가 없으면 0
	 */
	uint32 GetSelectedComponentUUID() const;

	/**
	 * @brief UUID 목록을 이용해 actor/component 선택 상태를 복원합니다.
	 *
	 * @param ActorUUIDs 선택할 actor UUID 목록
	 *
	 * @param ComponentUUID 선택할 component UUID. 0이면 primary actor root component 사용
	 */
	void RestoreSelectionByUUIDs(const TArray<uint32>& ActorUUIDs, uint32 ComponentUUID);
	bool IsEmpty() const { return SelectedActors.empty(); }

	UGizmoComponent* GetGizmo() const { return Gizmo; }

	void SetGizmoEnabled(bool bEnabled);
	void SetWorld(UWorld* InWorld);

	// GC
	void AddReferencedObjects(FReferenceCollector& Collector);
private:
	void SyncGizmo();
	void SetActorProxiesSelected(AActor* Actor, bool bSelected);

	TArray<AActor*> SelectedActors;
	USceneComponent* SelectedComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	UWorld* World = nullptr;
	bool bGizmoEnabled = true;
};
