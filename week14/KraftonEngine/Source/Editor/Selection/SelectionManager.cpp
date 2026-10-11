#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/FScene.h"
#include "Object/ReferenceCollector.h"

namespace
{
	bool ContainsUInt(const TArray<uint32>& Values, uint32 Value)
	{
		return std::find(Values.begin(), Values.end(), Value) != Values.end();
	}

	void AddUniqueUInt(TArray<uint32>& Values, uint32 Value)
	{
		if (Value != 0 && !ContainsUInt(Values, Value))
		{
			Values.push_back(Value);
		}
	}

	void RemoveUInt(TArray<uint32>& Values, uint32 Value)
	{
		Values.erase(std::remove(Values.begin(), Values.end(), Value), Values.end());
	}
}

void FSelectionManager::Init()
{
	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	Gizmo->Deactivate();
}

void FSelectionManager::SetWorld(UWorld* InWorld)
{
	for (AActor* Prev : SelectedActors)
	{
		SetActorProxiesSelected(Prev, false);
	}
	SelectedActors.clear();
	SelectedComponent = nullptr;

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

	RebuildSelectionFromState();
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
	if (Actor && DirectSelectedActorUUIDs.size() == 1 && DirectSelectedActorUUIDs.front() == Actor->GetUUID()
		&& SelectedGroupIds.empty() && SelectedActors.size() == 1 && SelectedActors.front() == Actor
		&& SelectedComponent == Actor->GetRootComponent())
	{
		return;
	}

	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();
	if (Actor)
	{
		AddUniqueUInt(DirectSelectedActorUUIDs, Actor->GetUUID());
	}
	RebuildSelectionFromState();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
	if (!ClickedActor) return;

	// Find index of clicked actor
	int32 ClickedIdx = -1;
	for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
	{
		if (ActorList[i] == ClickedActor) { ClickedIdx = i; break; }
	}
	if (ClickedIdx == -1) return;

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

	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();

	for (int32 i = Lo; i <= Hi; ++i)
	{
		if (ActorList[i])
		{
			AddUniqueUInt(DirectSelectedActorUUIDs, ActorList[i]->GetUUID());
		}
	}

	RebuildSelectionFromState();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
	if (!Actor) return;

	const uint32 ActorUUID = Actor->GetUUID();
	if (ContainsUInt(DirectSelectedActorUUIDs, ActorUUID))
	{
		RemoveUInt(DirectSelectedActorUUIDs, ActorUUID);
	}
	else
	{
		AddUniqueUInt(DirectSelectedActorUUIDs, ActorUUID);
	}
	RebuildSelectionFromState();
}

void FSelectionManager::SelectGroup(uint32 GroupId)
{
	if (GroupId == 0)
	{
		return;
	}

	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();
	AddUniqueUInt(SelectedGroupIds, GroupId);
	RebuildSelectionFromState();
}

void FSelectionManager::ToggleSelectGroup(uint32 GroupId)
{
	if (GroupId == 0)
	{
		return;
	}

	if (ContainsUInt(SelectedGroupIds, GroupId))
	{
		RemoveUInt(SelectedGroupIds, GroupId);
	}
	else
	{
		AddUniqueUInt(SelectedGroupIds, GroupId);
	}
	RebuildSelectionFromState();
}

void FSelectionManager::Deselect(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	RemoveUInt(DirectSelectedActorUUIDs, Actor->GetUUID());
	RebuildSelectionFromState();
}

int32 FSelectionManager::SelectAllActors()
{
	if (!World)
	{
		return 0;
	}

	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();

	// 현재 editor world에 속한 valid actor만 선택 대상에 포함
	for (AActor* Actor : World->GetActors())
	{
		if (!IsValid(Actor) || Actor->GetWorld() != World)
		{
			continue;
		}

		AddUniqueUInt(DirectSelectedActorUUIDs, Actor->GetUUID());
	}

	RebuildSelectionFromState();
	return static_cast<int32>(SelectedActors.size());
}

void FSelectionManager::ClearSelection()
{
	if (SelectedActors.empty() && SelectedComponent == nullptr && DirectSelectedActorUUIDs.empty() && SelectedGroupIds.empty())
	{
		return;
	}

	for (AActor* Actor : SelectedActors)
		SetActorProxiesSelected(Actor, false);

	SelectedActors.clear();
	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();
	SelectedComponent = nullptr;
	SyncGizmo();
}

TArray<uint32> FSelectionManager::GetSelectedActorUUIDs() const
{
	TArray<uint32> ActorUUIDs;
	for (AActor* Actor : SelectedActors)
	{
		if (Actor)
		{
			ActorUUIDs.push_back(Actor->GetUUID());
		}
	}
	return ActorUUIDs;
}

uint32 FSelectionManager::GetSelectedComponentUUID() const
{
	return SelectedComponent ? SelectedComponent->GetUUID() : 0;
}

void FSelectionManager::RestoreSelectionByUUIDs(const TArray<uint32>& ActorUUIDs, uint32 ComponentUUID)
{
	TArray<uint32> EmptyGroupIds;
	RestoreSelectionByUUIDs(ActorUUIDs, ComponentUUID, EmptyGroupIds);
}

void FSelectionManager::RestoreSelectionByUUIDs(const TArray<uint32>& ActorUUIDs, uint32 ComponentUUID, const TArray<uint32>& GroupIds)
{
	// 기존 선택 프록시를 먼저 해제해 복원 대상과 stale 대상이 섞이지 않도록 합니다.
	for (AActor* Prev : SelectedActors)
	{
		SetActorProxiesSelected(Prev, false);
	}
	SelectedActors.clear();
	SelectedComponent = nullptr;
	DirectSelectedActorUUIDs.clear();
	SelectedGroupIds.clear();

	for (uint32 ActorUUID : ActorUUIDs)
	{
		if (ActorUUID != 0)
		{
			AddUniqueUInt(DirectSelectedActorUUIDs, ActorUUID);
		}
	}

	for (uint32 GroupId : GroupIds)
	{
		if (GroupId != 0)
		{
			AddUniqueUInt(SelectedGroupIds, GroupId);
		}
	}

	RebuildSelectionFromState();

	if (ComponentUUID != 0)
	{
		USceneComponent* Component = Cast<USceneComponent>(UObjectManager::Get().FindByUUID(ComponentUUID));
		AActor* Owner = Component ? Component->GetOwner() : nullptr;
		if (Owner && std::find(SelectedActors.begin(), SelectedActors.end(), Owner) != SelectedActors.end())
		{
			SelectedComponent = Component;
		}
	}

	// component snapshot이 유효하지 않으면 primary actor의 root를 fallback target으로 사용합니다.
	if (!SelectedComponent && !SelectedActors.empty())
	{
		SelectedComponent = SelectedActors.front()->GetRootComponent();
	}

	SyncGizmo();
}

int32 FSelectionManager::DeleteSelectedActors()
{
	if (!World || SelectedActors.empty())
	{
		return 0;
	}

	TArray<AActor*> ActorsToDelete = SelectedActors;
	const int32 DeletedCount = static_cast<int32>(ActorsToDelete.size());

	// 파괴 전에 선택/기즈모 참조를 먼저 끊어 dangling target을 방지한다.
	ClearSelection();

	World->BeginDeferredPickingBVHUpdate();
	for (AActor* Actor : ActorsToDelete)
	{
		if (!Actor)
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
	if (!Gizmo || !bGizmoEnabled)
	{
		return;
	}

	USceneComponent* Primary = SelectedComponent;
	if (!Primary)
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

void FSelectionManager::SetGizmoEnabled(bool bEnabled)
{
	if (bGizmoEnabled == bEnabled)
	{
		return;
	}

	bGizmoEnabled = bEnabled;
	SyncGizmo();
}

void FSelectionManager::SelectComponent(USceneComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// [버그 수정] 에디터 전용 컴포넌트(광원 아이콘 등)는 개별 조작 대상이 아니므로,
	// 부모 컴포넌트로 리다이렉트하여 함께 움직이도록 합니다.
	USceneComponent* Target = Component;
	if (Component->IsEditorOnlyComponent())
	{
		if (Component->GetParent())
		{
			Target = Component->GetParent();
		}
		else
		{
			Target = Component->GetOwner()->GetRootComponent();
		}
	}

	if (Target)
	{
		AActor* Owner = Target->GetOwner();
		if (Owner && !IsSelected(Owner))
		{
			Select(Owner);
		}
	}

	if (SelectedComponent == Target)
	{
		return;
	}

	SelectedComponent = Target;
	SyncGizmo();
}

AActor* FSelectionManager::ResolveActorByUUID(uint32 ActorUUID) const
{
	if (ActorUUID == 0)
	{
		return nullptr;
	}

	AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
	return Actor && (!World || Actor->GetWorld() == World) ? Actor : nullptr;
}

void FSelectionManager::AddSelectedActorIfValid(AActor* Actor)
{
	if (!Actor || (World && Actor->GetWorld() != World))
	{
		return;
	}

	if (std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end())
	{
		return;
	}

	SelectedActors.push_back(Actor);
	SetActorProxiesSelected(Actor, true);
}

void FSelectionManager::RebuildSelectionFromState()
{
	for (AActor* Prev : SelectedActors)
	{
		SetActorProxiesSelected(Prev, false);
	}
	SelectedActors.clear();
	SelectedComponent = nullptr;

	DirectSelectedActorUUIDs.erase(
		std::remove_if(
			DirectSelectedActorUUIDs.begin(),
			DirectSelectedActorUUIDs.end(),
			[this](uint32 ActorUUID)
			{
				return ResolveActorByUUID(ActorUUID) == nullptr;
			}),
		DirectSelectedActorUUIDs.end());

	if (World)
	{
		FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
		SelectedGroupIds.erase(
			std::remove_if(
				SelectedGroupIds.begin(),
				SelectedGroupIds.end(),
				[&OutlinerState](uint32 GroupId)
				{
					return GroupId == 0 || !OutlinerState.FindGroup(GroupId);
				}),
			SelectedGroupIds.end());

		for (uint32 ActorUUID : DirectSelectedActorUUIDs)
		{
			AddSelectedActorIfValid(ResolveActorByUUID(ActorUUID));
		}

		TArray<uint32> GroupActorUUIDs = OutlinerState.GetActorUUIDsForGroups(SelectedGroupIds);
		for (uint32 ActorUUID : GroupActorUUIDs)
		{
			AddSelectedActorIfValid(ResolveActorByUUID(ActorUUID));
		}
	}

	if (!SelectedActors.empty())
	{
		SelectedComponent = SelectedActors.front()->GetRootComponent();
	}

	SyncGizmo();
}

void FSelectionManager::SetActorProxiesSelected(AActor* Actor, bool bSelected)
{
	if (!Actor || !World) return;
	if (Actor->GetWorld() != World) return;

	FScene& Scene = World->GetScene();
	for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
	{
		if (FPrimitiveSceneProxy* Proxy = Prim->GetSceneProxy())
		{
			Scene.SetProxySelected(Proxy, bSelected);
		}
	}
}

void FSelectionManager::SyncGizmo()
{
	if (!Gizmo) return;

	if (!bGizmoEnabled)
	{
		Gizmo->Deactivate();
		return;
	}

	USceneComponent* Primary = SelectedComponent;
	if (Primary)
	{
		Gizmo->SetSelectedActors(&SelectedActors);
		Gizmo->SetSelectedGroups(World, &SelectedGroupIds);
		Gizmo->SetTarget(Primary);
	}
	else
	{
		Gizmo->SetSelectedActors(nullptr);
		Gizmo->SetSelectedGroups(nullptr, nullptr);
		Gizmo->Deactivate();
	}
}

void FSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Gizmo);
}
