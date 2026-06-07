#include "Editor/Undo/EditorUndoCommand.h"

#include "Component/ActorComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float TransformTolerance = 1.0e-4f;

/**
 * @brief actor JSON과 runtime UUID를 함께 보관하는 undo용 스냅샷
 */
struct FEditorActorSnapshot
{
	uint32 ActorUUID = 0;
	json::JSON ActorJSON;
};

bool IsNearlyEqual(float A, float B)
{
	return std::fabs(A - B) <= TransformTolerance;
}

bool AreVectorsNearlyEqual(const FVector& A, const FVector& B)
{
	return IsNearlyEqual(A.X, B.X)
		&& IsNearlyEqual(A.Y, B.Y)
		&& IsNearlyEqual(A.Z, B.Z);
}

bool AreQuatsNearlyEqual(const FQuat& A, const FQuat& B)
{
	return A.Equals(B, TransformTolerance)
		|| FQuat(-A.X, -A.Y, -A.Z, -A.W).Equals(B, TransformTolerance);
}

bool AreTransformsNearlyEqual(const FTransform& A, const FTransform& B)
{
	return AreVectorsNearlyEqual(A.Location, B.Location)
		&& AreVectorsNearlyEqual(A.Scale, B.Scale)
		&& AreQuatsNearlyEqual(A.Rotation.GetNormalized(), B.Rotation.GetNormalized());
}

UWorld* GetEditorWorld(UEditorEngine* EditorEngine)
{
	return EditorEngine ? EditorEngine->GetWorld() : nullptr;
}

AActor* ResolveActorByUUID(UWorld* World, uint32 ActorUUID)
{
	if (!World || ActorUUID == 0)
	{
		return nullptr;
	}

	AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
	return Actor && Actor->GetWorld() == World ? Actor : nullptr;
}

USceneComponent* ResolveSceneComponentByUUID(UEditorEngine* EditorEngine, uint32 ComponentUUID)
{
	if (!EditorEngine || ComponentUUID == 0)
	{
		return nullptr;
	}

	USceneComponent* Component = Cast<USceneComponent>(UObjectManager::Get().FindByUUID(ComponentUUID));
	if (!Component)
	{
		return nullptr;
	}

	AActor* Owner = Component->GetOwner();
	UWorld* World = GetEditorWorld(EditorEngine);
	return Owner && World && Owner->GetWorld() == World ? Component : nullptr;
}

UObject* ResolveObjectByUUID(UEditorEngine* EditorEngine, uint32 ObjectUUID)
{
	if (!EditorEngine || ObjectUUID == 0)
	{
		return nullptr;
	}

	UObject* Object = UObjectManager::Get().FindByUUID(ObjectUUID);
	if (!IsValid(Object))
	{
		return nullptr;
	}

	// 현재 editor world에 속한 객체만 속성 undo 대상으로 인정합니다.
	UWorld* World = GetEditorWorld(EditorEngine);
	if (!World)
	{
		return nullptr;
	}

	if (AActor* Actor = Cast<AActor>(Object))
	{
		return Actor->GetWorld() == World ? Actor : nullptr;
	}

	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		AActor* Owner = Component->GetOwner();
		return Owner && Owner->GetWorld() == World ? Component : nullptr;
	}

	return Object->GetTypedOuter<UWorld>() == World ? Object : nullptr;
}

void AddUniqueActorUUID(TArray<uint32>& ActorUUIDs, AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const uint32 ActorUUID = Actor->GetUUID();
	if (std::find(ActorUUIDs.begin(), ActorUUIDs.end(), ActorUUID) == ActorUUIDs.end())
	{
		ActorUUIDs.push_back(ActorUUID);
	}
}

void AddUniqueComponentTransformSnapshot(
	TArray<FEditorComponentTransformSnapshot>& Snapshots,
	USceneComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const uint32 ComponentUUID = Component->GetUUID();
	for (const FEditorComponentTransformSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.ComponentUUID == ComponentUUID)
		{
			return;
		}
	}

	FEditorComponentTransformSnapshot Snapshot;
	Snapshot.ComponentUUID = ComponentUUID;
	Snapshot.RelativeTransform = Component->GetRelativeTransform();
	Snapshots.push_back(Snapshot);
}

FEditorActorSnapshot CaptureActorSnapshot(AActor* Actor)
{
	FEditorActorSnapshot Snapshot;
	if (!Actor)
	{
		return Snapshot;
	}

	Snapshot.ActorUUID = Actor->GetUUID();
	Snapshot.ActorJSON = FSceneSaveManager::SerializeActorForEditorUndo(Actor);
	return Snapshot;
}

bool AreObjectPropertySnapshotsEqual(
	const TArray<FEditorObjectPropertySnapshot>& A,
	const TArray<FEditorObjectPropertySnapshot>& B)
{
	if (A.size() != B.size())
	{
		return false;
	}

	for (size_t Index = 0; Index < A.size(); ++Index)
	{
		if (A[Index].ObjectUUID != B[Index].ObjectUUID)
		{
			return false;
		}

		json::JSON AJSON = A[Index].ObjectJSON;
		json::JSON BJSON = B[Index].ObjectJSON;
		const FString ADump = AJSON.hasKey("Properties") ? AJSON["Properties"].dump() : AJSON.dump();
		const FString BDump = BJSON.hasKey("Properties") ? BJSON["Properties"].dump() : BJSON.dump();
		if (ADump != BDump)
		{
			return false;
		}
	}

	return true;
}

bool AreActorStructureSnapshotsEqual(json::JSON A, json::JSON B)
{
	const FString ARoot = A.hasKey("RootComponent") ? A["RootComponent"].dump() : FString();
	const FString BRoot = B.hasKey("RootComponent") ? B["RootComponent"].dump() : FString();
	if (ARoot != BRoot)
	{
		return false;
	}

	const FString ANonScene = A.hasKey("NonSceneComponents") ? A["NonSceneComponents"].dump() : FString();
	const FString BNonScene = B.hasKey("NonSceneComponents") ? B["NonSceneComponents"].dump() : FString();
	return ANonScene == BNonScene;
}

void ApplyEditorSelection(UEditorEngine* EditorEngine, const FEditorSelectionSnapshot& Selection)
{
	if (!EditorEngine)
	{
		return;
	}

	// Undo/Redo 적용 후 selection과 gizmo target을 UUID 기준으로 다시 바인딩합니다.
	FSelectionManager& SelectionManager = EditorEngine->GetSelectionManager();
	SelectionManager.RestoreSelectionByUUIDs(Selection.ActorUUIDs, Selection.ComponentUUID, Selection.GroupIds);

	if (UGizmoComponent* Gizmo = EditorEngine->GetGizmo())
	{
		Gizmo->UpdateGizmoTransform();
	}
}

void RefreshEditorWorldAfterCommand(UEditorEngine* EditorEngine)
{
	if (!EditorEngine)
	{
		return;
	}

	if (UWorld* World = GetEditorWorld(EditorEngine))
	{
		World->BuildWorldPrimitivePickingBVHNow();
	}
	EditorEngine->InvalidateOcclusionResults();
}

void DestroyActorsByUUIDs(UEditorEngine* EditorEngine, const TArray<uint32>& ActorUUIDs)
{
	UWorld* World = GetEditorWorld(EditorEngine);
	if (!World || ActorUUIDs.empty())
	{
		return;
	}

	// 삭제 대상 actor/component를 선택 상태에서 먼저 끊어 dangling gizmo target을 방지합니다.
	EditorEngine->GetSelectionManager().ClearSelection();

	World->BeginDeferredPickingBVHUpdate();
	for (uint32 ActorUUID : ActorUUIDs)
	{
		if (AActor* Actor = ResolveActorByUUID(World, ActorUUID))
		{
			World->DestroyActor(Actor);
		}
	}
	World->EndDeferredPickingBVHUpdate();
}

void RestoreActorsFromSnapshots(UEditorEngine* EditorEngine, const TArray<FEditorActorSnapshot>& Snapshots)
{
	UWorld* World = GetEditorWorld(EditorEngine);
	if (!World)
	{
		return;
	}

	for (const FEditorActorSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.ActorUUID == 0 || ResolveActorByUUID(World, Snapshot.ActorUUID))
		{
			continue;
		}

		FSceneSaveManager::DeserializeActorForEditorUndo(World, Snapshot.ActorJSON);
	}
}

void ApplyComponentTransformSnapshots(
	UEditorEngine* EditorEngine,
	const TArray<FEditorComponentTransformSnapshot>& Snapshots)
{
	for (const FEditorComponentTransformSnapshot& Snapshot : Snapshots)
	{
		USceneComponent* Component = ResolveSceneComponentByUUID(EditorEngine, Snapshot.ComponentUUID);
		if (!Component)
		{
			continue;
		}

		// Quat 기반 회전 setter를 통과시켜 Details 패널의 Euler cache도 dirty 처리합니다.
		Component->SetRelativeLocation(Snapshot.RelativeTransform.Location);
		Component->SetRelativeRotation(Snapshot.RelativeTransform.Rotation);
		Component->SetRelativeScale(Snapshot.RelativeTransform.Scale);
	}
}

void ApplyObjectPropertySnapshots(
	UEditorEngine* EditorEngine,
	const TArray<FEditorObjectPropertySnapshot>& Snapshots)
{
	for (const FEditorObjectPropertySnapshot& Snapshot : Snapshots)
	{
		UObject* Object = ResolveObjectByUUID(EditorEngine, Snapshot.ObjectUUID);
		if (!Object)
		{
			continue;
		}

		// DeserializeProperties 내부의 PostEditChangeProperty 호출이 실제 render/physics/editor 반영을 수행합니다.
		FSceneSaveManager::DeserializeObjectPropertiesForEditorUndo(Object, Snapshot.ObjectJSON);
	}
}

class FActorCreateUndoCommand : public IEditorUndoCommand
{
public:
	FActorCreateUndoCommand(
		TArray<FEditorActorSnapshot> InActorSnapshots,
		FEditorSelectionSnapshot InSelectionBefore,
		FEditorSelectionSnapshot InSelectionAfter,
		FString InDebugName)
		: ActorSnapshots(std::move(InActorSnapshots))
		, SelectionBefore(std::move(InSelectionBefore))
		, SelectionAfter(std::move(InSelectionAfter))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		// 생성 취소: 새로 생긴 actor들을 제거하고 이전 선택 상태를 복구합니다.
		DestroyActorsByUUIDs(EditorEngine, GetActorUUIDs());
		ApplyEditorSelection(EditorEngine, SelectionBefore);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		// 생성 재적용: actor snapshot을 다시 복원하고 생성 직후 선택 상태를 복구합니다.
		RestoreActorsFromSnapshots(EditorEngine, ActorSnapshots);
		ApplyEditorSelection(EditorEngine, SelectionAfter);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	TArray<uint32> GetActorUUIDs() const
	{
		TArray<uint32> ActorUUIDs;
		for (const FEditorActorSnapshot& Snapshot : ActorSnapshots)
		{
			if (Snapshot.ActorUUID != 0)
			{
				ActorUUIDs.push_back(Snapshot.ActorUUID);
			}
		}
		return ActorUUIDs;
	}

	TArray<FEditorActorSnapshot> ActorSnapshots;
	FEditorSelectionSnapshot SelectionBefore;
	FEditorSelectionSnapshot SelectionAfter;
	FString DebugName;
};

class FActorDeleteUndoCommand : public IEditorUndoCommand
{
public:
	FActorDeleteUndoCommand(
		TArray<FEditorActorSnapshot> InActorSnapshots,
		FEditorSelectionSnapshot InSelectionBefore,
		FEditorSelectionSnapshot InSelectionAfter,
		FString InDebugName)
		: ActorSnapshots(std::move(InActorSnapshots))
		, SelectionBefore(std::move(InSelectionBefore))
		, SelectionAfter(std::move(InSelectionAfter))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		// 삭제 취소: 삭제 전 actor snapshot을 복원하고 삭제 전 선택 상태를 복구합니다.
		RestoreActorsFromSnapshots(EditorEngine, ActorSnapshots);
		ApplyEditorSelection(EditorEngine, SelectionBefore);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		// 삭제 재적용: 같은 UUID의 actor들을 다시 제거하고 삭제 후 선택 상태를 복구합니다.
		DestroyActorsByUUIDs(EditorEngine, GetActorUUIDs());
		ApplyEditorSelection(EditorEngine, SelectionAfter);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	TArray<uint32> GetActorUUIDs() const
	{
		TArray<uint32> ActorUUIDs;
		for (const FEditorActorSnapshot& Snapshot : ActorSnapshots)
		{
			if (Snapshot.ActorUUID != 0)
			{
				ActorUUIDs.push_back(Snapshot.ActorUUID);
			}
		}
		return ActorUUIDs;
	}

	TArray<FEditorActorSnapshot> ActorSnapshots;
	FEditorSelectionSnapshot SelectionBefore;
	FEditorSelectionSnapshot SelectionAfter;
	FString DebugName;
};

class FActorStructureUndoCommand : public IEditorUndoCommand
{
public:
	FActorStructureUndoCommand(
		uint32 InActorUUID,
		json::JSON InBeforeActorJSON,
		json::JSON InAfterActorJSON,
		FEditorSelectionSnapshot InSelectionBefore,
		FEditorSelectionSnapshot InSelectionAfter,
		FString InDebugName)
		: ActorUUID(InActorUUID)
		, BeforeActorJSON(std::move(InBeforeActorJSON))
		, AfterActorJSON(std::move(InAfterActorJSON))
		, SelectionBefore(std::move(InSelectionBefore))
		, SelectionAfter(std::move(InSelectionAfter))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		// component 구조 변경 취소: 기존 actor는 유지하고 before snapshot의 component 구조만 복원합니다.
		ApplySnapshot(EditorEngine, BeforeActorJSON, SelectionBefore);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		// component 구조 변경 재적용: 같은 actor에 after snapshot의 component 구조를 다시 적용합니다.
		ApplySnapshot(EditorEngine, AfterActorJSON, SelectionAfter);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	void ApplySnapshot(
		UEditorEngine* EditorEngine,
		json::JSON ActorJSON,
		const FEditorSelectionSnapshot& Selection)
	{
		UWorld* World = GetEditorWorld(EditorEngine);
		AActor* Actor = ResolveActorByUUID(World, ActorUUID);
		if (!Actor)
		{
			return;
		}

		// 기존 selection/gizmo가 곧 삭제될 component를 가리킬 수 있으므로 먼저 참조를 끊습니다.
		EditorEngine->GetSelectionManager().ClearSelection();
		FSceneSaveManager::ApplyActorSnapshotForEditorUndo(Actor, std::move(ActorJSON));
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	uint32 ActorUUID = 0;
	json::JSON BeforeActorJSON;
	json::JSON AfterActorJSON;
	FEditorSelectionSnapshot SelectionBefore;
	FEditorSelectionSnapshot SelectionAfter;
	FString DebugName;
};

class FTransformUndoCommand : public IEditorUndoCommand
{
public:
	FTransformUndoCommand(
		TArray<FEditorComponentTransformSnapshot> InBefore,
		TArray<FEditorComponentTransformSnapshot> InAfter,
		FEditorSelectionSnapshot InSelection,
		FString InDebugName)
		: Before(std::move(InBefore))
		, After(std::move(InAfter))
		, Selection(std::move(InSelection))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		ApplyComponentTransformSnapshots(EditorEngine, Before);
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		ApplyComponentTransformSnapshots(EditorEngine, After);
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	TArray<FEditorComponentTransformSnapshot> Before;
	TArray<FEditorComponentTransformSnapshot> After;
	FEditorSelectionSnapshot Selection;
	FString DebugName;
};

class FObjectPropertyUndoCommand : public IEditorUndoCommand
{
public:
	FObjectPropertyUndoCommand(
		TArray<FEditorObjectPropertySnapshot> InBefore,
		TArray<FEditorObjectPropertySnapshot> InAfter,
		FEditorSelectionSnapshot InSelection,
		FString InDebugName)
		: Before(std::move(InBefore))
		, After(std::move(InAfter))
		, Selection(std::move(InSelection))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		// Details 속성 변경 취소: 기록된 before 속성 스냅샷을 같은 UUID의 객체에 적용합니다.
		ApplyObjectPropertySnapshots(EditorEngine, Before);
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		// Details 속성 변경 재적용: 기록된 after 속성 스냅샷을 같은 UUID의 객체에 적용합니다.
		ApplyObjectPropertySnapshots(EditorEngine, After);
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	TArray<FEditorObjectPropertySnapshot> Before;
	TArray<FEditorObjectPropertySnapshot> After;
	FEditorSelectionSnapshot Selection;
	FString DebugName;
};

class FObjectRenameUndoCommand : public IEditorUndoCommand
{
public:
	FObjectRenameUndoCommand(
		uint32 InObjectUUID,
		FName InBeforeName,
		FName InAfterName,
		FEditorSelectionSnapshot InSelection,
		FString InDebugName)
		: ObjectUUID(InObjectUUID)
		, BeforeName(std::move(InBeforeName))
		, AfterName(std::move(InAfterName))
		, Selection(std::move(InSelection))
		, DebugName(std::move(InDebugName))
	{
	}

	void Undo(UEditorEngine* EditorEngine) override
	{
		// 이름 변경 취소: actor/component tree가 같은 객체 포인터를 표시하도록 UUID로 다시 찾습니다.
		if (UObject* Object = ResolveObjectByUUID(EditorEngine, ObjectUUID))
		{
			Object->SetFName(BeforeName);
		}
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	void Redo(UEditorEngine* EditorEngine) override
	{
		// 이름 변경 재적용: undo와 같은 대상 객체에 새 이름을 다시 지정합니다.
		if (UObject* Object = ResolveObjectByUUID(EditorEngine, ObjectUUID))
		{
			Object->SetFName(AfterName);
		}
		ApplyEditorSelection(EditorEngine, Selection);
		RefreshEditorWorldAfterCommand(EditorEngine);
	}

	FString GetDebugName() const override
	{
		return DebugName;
	}

private:
	uint32 ObjectUUID = 0;
	FName BeforeName;
	FName AfterName;
	FEditorSelectionSnapshot Selection;
	FString DebugName;
};
}

FEditorSelectionSnapshot CaptureEditorSelection(const FSelectionManager* SelectionManager)
{
	FEditorSelectionSnapshot Snapshot;
	if (!SelectionManager)
	{
		return Snapshot;
	}

	for (uint32 ActorUUID : SelectionManager->GetDirectSelectedActorUUIDs())
	{
		if (ActorUUID != 0 && std::find(Snapshot.ActorUUIDs.begin(), Snapshot.ActorUUIDs.end(), ActorUUID) == Snapshot.ActorUUIDs.end())
		{
			Snapshot.ActorUUIDs.push_back(ActorUUID);
		}
	}

	for (uint32 GroupId : SelectionManager->GetSelectedGroupIds())
	{
		if (GroupId != 0 && std::find(Snapshot.GroupIds.begin(), Snapshot.GroupIds.end(), GroupId) == Snapshot.GroupIds.end())
		{
			Snapshot.GroupIds.push_back(GroupId);
		}
	}

	if (USceneComponent* Component = SelectionManager->GetSelectedComponent())
	{
		Snapshot.ComponentUUID = Component->GetUUID();
	}

	return Snapshot;
}

TArray<FEditorComponentTransformSnapshot> CaptureSelectedComponentTransforms(const FSelectionManager* SelectionManager)
{
	TArray<FEditorComponentTransformSnapshot> Snapshots;
	if (!SelectionManager)
	{
		return Snapshots;
	}

	// 단일 component 선택과 다중 actor translate 양쪽을 모두 커버하기 위해
	// selected component와 선택 actor root들을 함께 기록합니다.
	AddUniqueComponentTransformSnapshot(Snapshots, SelectionManager->GetSelectedComponent());
	for (AActor* Actor : SelectionManager->GetSelectedActors())
	{
		if (Actor)
		{
			AddUniqueComponentTransformSnapshot(Snapshots, Actor->GetRootComponent());
		}
	}

	return Snapshots;
}

TArray<FEditorObjectPropertySnapshot> CaptureObjectPropertySnapshots(const TArray<UObject*>& Objects)
{
	TArray<FEditorObjectPropertySnapshot> Snapshots;
	for (UObject* Object : Objects)
	{
		if (!IsValid(Object))
		{
			continue;
		}

		const uint32 ObjectUUID = Object->GetUUID();
		if (ObjectUUID == 0)
		{
			continue;
		}

		bool bAlreadyCaptured = false;
		for (const FEditorObjectPropertySnapshot& Existing : Snapshots)
		{
			if (Existing.ObjectUUID == ObjectUUID)
			{
				bAlreadyCaptured = true;
				break;
			}
		}
		if (bAlreadyCaptured)
		{
			continue;
		}

		FEditorObjectPropertySnapshot Snapshot;
		Snapshot.ObjectUUID = ObjectUUID;
		Snapshot.ObjectJSON = FSceneSaveManager::SerializeObjectPropertiesForEditorUndo(Object);
		Snapshots.push_back(std::move(Snapshot));
	}

	return Snapshots;
}

bool AreComponentTransformSnapshotsEqual(
	const TArray<FEditorComponentTransformSnapshot>& A,
	const TArray<FEditorComponentTransformSnapshot>& B)
{
	if (A.size() != B.size())
	{
		return false;
	}

	for (size_t Index = 0; Index < A.size(); ++Index)
	{
		if (A[Index].ComponentUUID != B[Index].ComponentUUID)
		{
			return false;
		}

		if (!AreTransformsNearlyEqual(A[Index].RelativeTransform, B[Index].RelativeTransform))
		{
			return false;
		}
	}

	return true;
}

std::unique_ptr<IEditorUndoCommand> MakeActorCreateUndoCommand(
	const TArray<AActor*>& Actors,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName)
{
	TArray<FEditorActorSnapshot> ActorSnapshots;
	for (AActor* Actor : Actors)
	{
		FEditorActorSnapshot Snapshot = CaptureActorSnapshot(Actor);
		if (Snapshot.ActorUUID != 0)
		{
			ActorSnapshots.push_back(std::move(Snapshot));
		}
	}

	if (ActorSnapshots.empty())
	{
		return nullptr;
	}

	return std::make_unique<FActorCreateUndoCommand>(
		std::move(ActorSnapshots),
		SelectionBefore,
		SelectionAfter,
		DebugName);
}

std::unique_ptr<IEditorUndoCommand> MakeActorDeleteUndoCommand(
	const TArray<AActor*>& Actors,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName)
{
	TArray<FEditorActorSnapshot> ActorSnapshots;
	for (AActor* Actor : Actors)
	{
		FEditorActorSnapshot Snapshot = CaptureActorSnapshot(Actor);
		if (Snapshot.ActorUUID != 0)
		{
			ActorSnapshots.push_back(std::move(Snapshot));
		}
	}

	if (ActorSnapshots.empty())
	{
		return nullptr;
	}

	return std::make_unique<FActorDeleteUndoCommand>(
		std::move(ActorSnapshots),
		SelectionBefore,
		SelectionAfter,
		DebugName);
}

std::unique_ptr<IEditorUndoCommand> MakeActorStructureUndoCommand(
	AActor* Actor,
	json::JSON BeforeActorJSON,
	json::JSON AfterActorJSON,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName)
{
	if (!IsValid(Actor) || Actor->GetUUID() == 0
		|| AreActorStructureSnapshotsEqual(BeforeActorJSON, AfterActorJSON))
	{
		return nullptr;
	}

	return std::make_unique<FActorStructureUndoCommand>(
		Actor->GetUUID(),
		std::move(BeforeActorJSON),
		std::move(AfterActorJSON),
		SelectionBefore,
		SelectionAfter,
		DebugName);
}

std::unique_ptr<IEditorUndoCommand> MakeTransformUndoCommand(
	const TArray<FEditorComponentTransformSnapshot>& Before,
	const TArray<FEditorComponentTransformSnapshot>& After,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName)
{
	if (Before.empty() || AreComponentTransformSnapshotsEqual(Before, After))
	{
		return nullptr;
	}

	return std::make_unique<FTransformUndoCommand>(Before, After, Selection, DebugName);
}

std::unique_ptr<IEditorUndoCommand> MakeObjectPropertyUndoCommand(
	const TArray<FEditorObjectPropertySnapshot>& Before,
	const TArray<FEditorObjectPropertySnapshot>& After,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName)
{
	if (Before.empty() || After.empty() || AreObjectPropertySnapshotsEqual(Before, After))
	{
		return nullptr;
	}

	return std::make_unique<FObjectPropertyUndoCommand>(Before, After, Selection, DebugName);
}

std::unique_ptr<IEditorUndoCommand> MakeObjectRenameUndoCommand(
	UObject* Object,
	const FName& BeforeName,
	const FName& AfterName,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName)
{
	if (!IsValid(Object) || Object->GetUUID() == 0 || BeforeName == AfterName)
	{
		return nullptr;
	}

	return std::make_unique<FObjectRenameUndoCommand>(
		Object->GetUUID(),
		BeforeName,
		AfterName,
		Selection,
		DebugName);
}
