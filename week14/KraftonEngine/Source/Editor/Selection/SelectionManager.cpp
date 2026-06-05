#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/FScene.h"
#include "Object/ReferenceCollector.h"

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
	if (SelectedActors.size() == 1 && SelectedActors.front() == Actor && (!Actor || SelectedComponent == Actor->GetRootComponent()))
	{
		return;
	}

	// 기존 선택 해제
	for (AActor* Prev : SelectedActors)
		SetActorProxiesSelected(Prev, false);

	SelectedActors.clear();
	SelectedComponent = nullptr;

	if (Actor)
	{
		SelectedActors.push_back(Actor);
		SetActorProxiesSelected(Actor, true);
		SelectedComponent = Actor->GetRootComponent();
	}
	SyncGizmo();
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

	// 기존 선택 해제
	for (AActor* Prev : SelectedActors)
		SetActorProxiesSelected(Prev, false);

	SelectedActors.clear();
	SelectedComponent = nullptr;

	for (int32 i = Lo; i <= Hi; ++i)
	{
		if (ActorList[i])
		{
			SelectedActors.push_back(ActorList[i]);
			SetActorProxiesSelected(ActorList[i], true);
		}
	}

	if (!SelectedActors.empty())
	{
		SelectedComponent = SelectedActors.front()->GetRootComponent();
	}

	SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SetActorProxiesSelected(Actor, false);
		SelectedActors.erase(It);
		if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
		{
			SelectedComponent = SelectedActors.empty() ? nullptr : SelectedActors.front()->GetRootComponent();
		}
	}
	else
	{
		SelectedActors.push_back(Actor);
		SetActorProxiesSelected(Actor, true);
		if (SelectedActors.size() == 1)
		{
			SelectedComponent = Actor->GetRootComponent();
		}
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
		if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
		{
			SelectedComponent = SelectedActors.empty() ? nullptr : SelectedActors.front()->GetRootComponent();
		}
	}
	SyncGizmo();
}

int32 FSelectionManager::SelectAllActors()
{
	if (!World)
	{
		return 0;
	}

	// 기존 선택 프록시 해제
	for (AActor* Prev : SelectedActors)
	{
		SetActorProxiesSelected(Prev, false);
	}
	SelectedActors.clear();
	SelectedComponent = nullptr;

	// 현재 editor world에 속한 valid actor만 선택 대상에 포함
	for (AActor* Actor : World->GetActors())
	{
		if (!IsValid(Actor) || Actor->GetWorld() != World)
		{
			continue;
		}

		SelectedActors.push_back(Actor);
		SetActorProxiesSelected(Actor, true);
	}

	// primary selection의 root component를 기즈모 기준으로 사용
	if (!SelectedActors.empty())
	{
		SelectedComponent = SelectedActors.front()->GetRootComponent();
	}

	SyncGizmo();
	return static_cast<int32>(SelectedActors.size());
}

void FSelectionManager::ClearSelection()
{
	if (SelectedActors.empty() && SelectedComponent == nullptr)
	{
		return;
	}

	for (AActor* Actor : SelectedActors)
		SetActorProxiesSelected(Actor, false);

	SelectedActors.clear();
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
	// 기존 선택 프록시를 먼저 해제해 복원 대상과 stale 대상이 섞이지 않도록 합니다.
	for (AActor* Prev : SelectedActors)
	{
		SetActorProxiesSelected(Prev, false);
	}
	SelectedActors.clear();
	SelectedComponent = nullptr;

	for (uint32 ActorUUID : ActorUUIDs)
	{
		AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
		if (!Actor || (World && Actor->GetWorld() != World))
		{
			continue;
		}

		if (std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end())
		{
			continue;
		}

		SelectedActors.push_back(Actor);
		SetActorProxiesSelected(Actor, true);
	}

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

	if (SelectedComponent == Target)
	{
		return;
	}

	SelectedComponent = Target;

	if (Target)
	{
		AActor* Owner = Target->GetOwner();
		if (Owner && !IsSelected(Owner))
		{
			Select(Owner);
		}
	}

	SyncGizmo();
}

void FSelectionManager::SetActorProxiesSelected(AActor* Actor, bool bSelected)
{
	if (!Actor || !World) return;

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
		Gizmo->SetTarget(Primary);
	}
	else
	{
		Gizmo->SetSelectedActors(nullptr);
		Gizmo->Deactivate();
	}
}

void FSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Gizmo);
}
