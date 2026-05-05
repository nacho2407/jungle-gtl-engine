#include "Editor/Selection/SelectionManager.h"

#include "Object/Object.h"
#include "Component/ActorComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"

void FSelectionManager::Init()
{
	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	Gizmo->Deactivate();
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
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;
	SelectedActors.clear();
	if (Actor)
	{
		SelectedActors.push_back(Actor);
	}
	RequestGizmoSync();
}

void FSelectionManager::AddSelect(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	SelectedActors.push_back(Actor);

	RequestGizmoSync();
}

namespace
{
	void PushUniqueSelection(TArray<AActor*>& OutActors, AActor* Actor)
	{
		if (Actor && std::find(OutActors.begin(), OutActors.end(), Actor) == OutActors.end())
		{
			OutActors.push_back(Actor);
		}
	}
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
	if (!ClickedActor) return;
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;

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

	SelectedActors.clear();
	for (int32 i = Lo; i <= Hi; ++i)
	{
		if (ActorList[i] != ClickedActor)
		{
			PushUniqueSelection(SelectedActors, ActorList[i]);
		}
	}
	PushUniqueSelection(SelectedActors, ClickedActor);
	RequestGizmoSync();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
	if (!Actor) return;
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	else
	{
		SelectedActors.push_back(Actor);
	}
	RequestGizmoSync();
}

void FSelectionManager::Deselect(AActor* Actor)
{
	ValidateSelection();
	if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
	{
		SelectedComponent = nullptr;
		SelectedComponentUUID = 0;
	}

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	RequestGizmoSync();
}

void FSelectionManager::ClearSelection()
{
	SelectedActors.clear();
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;
	RequestGizmoSync();
}

void FSelectionManager::SelectComponent(UActorComponent* Component)
{
	SelectedComponent = Component;
	SelectedComponentUUID = Component ? Component->GetUUID() : 0;
	if (Component && Component->GetOwner())
	{
		SelectedActors.clear();
		SelectedActors.push_back(Component->GetOwner());
	}
	RequestGizmoSync();
}

void FSelectionManager::ClearComponentSelection()
{
	if (!SelectedComponent)
	{
		return;
	}
	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;
	RequestGizmoSync();
}

void FSelectionManager::OnComponentDestroyed(UActorComponent* Component)
{
	if (!Component || SelectedComponent != Component)
	{
		return;
	}

	SelectedComponent = nullptr;
	SelectedComponentUUID = 0;
	RequestGizmoSync();
}

bool FSelectionManager::IsSelectedComponentAlive() const
{
	if (!SelectedComponent || SelectedComponentUUID == 0)
	{
		return false;
	}

	if (!UObjectManager::Get().ContainsObject(SelectedComponent))
	{
		return false;
	}

	return SelectedComponent->GetUUID() == SelectedComponentUUID;
}

void FSelectionManager::ValidateSelection()
{
	if (SelectedComponent && !IsSelectedComponentAlive())
	{
		SelectedComponent = nullptr;
		SelectedComponentUUID = 0;
		RequestGizmoSync();
	}
}

void FSelectionManager::BeginBatchUpdate()
{
	++BatchUpdateDepth;
}

void FSelectionManager::EndBatchUpdate()
{
	if (BatchUpdateDepth <= 0)
	{
		SyncGizmo();
		return;
	}

	--BatchUpdateDepth;
	if (BatchUpdateDepth == 0 && bPendingGizmoSync)
	{
		bPendingGizmoSync = false;
		SyncGizmo();
	}
}

void FSelectionManager::OnActorDestroyed(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

	ValidateSelection();

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SelectedActors.erase(It);

        RequestGizmoSync();
    }

	if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
	{
		SelectedComponent = nullptr;
		SelectedComponentUUID = 0;
		RequestGizmoSync();
	}
}

void FSelectionManager::RequestGizmoSync()
{
	if (BatchUpdateDepth > 0)
	{
		bPendingGizmoSync = true;
		return;
	}

	SyncGizmo();
}

void FSelectionManager::SyncGizmo()
{
	if (!Gizmo) return;
	ValidateSelection();

	AActor* Primary = GetPrimarySelection();
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(SelectedComponent))
	{
		Gizmo->SetSelectedActors(nullptr);
		Gizmo->SetTargetComponent(SceneComponent);
	}
	else if (Primary)
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
