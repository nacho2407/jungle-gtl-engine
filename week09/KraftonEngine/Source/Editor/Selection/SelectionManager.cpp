#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/GizmoComponent.h"

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
	SelectedActors.clear();
	if (Actor)
	{
		SelectedActors.push_back(Actor);
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

	SelectedActors.clear();
	for (int32 i = Lo; i <= Hi; ++i)
	{
		if (ActorList[i])
		{
			SelectedActors.push_back(ActorList[i]);
		}
	}
	SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	else
	{
		SelectedActors.push_back(Actor);
	}
	SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
	SelectedActors.clear();
	SyncGizmo();
}

void FSelectionManager::SyncGizmo()
{
	if (!Gizmo) return;

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
