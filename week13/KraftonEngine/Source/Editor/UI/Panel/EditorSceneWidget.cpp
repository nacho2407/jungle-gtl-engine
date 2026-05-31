#include "Editor/UI/Panel/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Component/ActorComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats/Stats.h"

namespace
{
	bool ShouldHideInComponentTree(const UActorComponent* Component, bool bShowEditorOnlyComponents)
	{
		if (!Component)
		{
			return true;
		}

		return Component->IsHiddenInComponentTree()
			&& !(bShowEditorOnlyComponents && Component->IsEditorOnlyComponent());
	}

	const char* GetActorDisplayName(AActor* Actor, FString& OutName)
	{
		OutName = Actor->GetFName().ToString();
		return OutName.empty()
			? Actor->GetClass()->GetName()
			: OutName.c_str();
	}

	const char* GetComponentDisplayName(UActorComponent* Component, FString& OutName)
	{
		OutName = Component->GetFName().ToString();
		const FString TypeName = Component->GetClass()->GetName();
		const FString DefaultNamePrefix = TypeName + "_";
		const bool bUseTypeAsLabel = OutName.empty() || OutName == TypeName || OutName.rfind(DefaultNamePrefix, 0) == 0;
		if (bUseTypeAsLabel)
		{
			OutName = TypeName;
		}
		return OutName.c_str();
	}
}

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorSceneWidget::Render(const FEditorPanelContext& Context)
{
	if (!EditorEngine || !Context.SelectionManager)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Scene Manager");

	// 씬 파일 작업은 상단 메뉴로 옮기고, Scene Manager는 액터 목록만 유지한다.
	RenderActorOutliner(*Context.SelectionManager);

	ImGui::End();
}

void FEditorSceneWidget::RenderActorOutliner(FSelectionManager& Selection)
{
	SCOPE_STAT_CAT("SceneWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World) return;

	const TArray<AActor*>& Actors = World->GetActors();

	// null이 아닌 유효 Actor 인덱스만 수집 (Clipper는 연속 인덱스 필요)
	ValidActorIndices.clear();
	ValidActorIndices.reserve(Actors.size());
	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		if (Actors[i]) ValidActorIndices.push_back(i);
	}

	ImGui::Text("Actors (%d)", static_cast<int32>(ValidActorIndices.size()));
	ImGui::Separator();

	ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

	for (int32 ActorIndex : ValidActorIndices)
	{
		RenderActorNode(Actors[ActorIndex], Actors, Selection);
	}

	ImGui::EndChild();
}

void FEditorSceneWidget::RenderActorNode(AActor* Actor, const TArray<AActor*>& Actors, FSelectionManager& Selection)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FString DisplayNameStorage;
	const char* DisplayName = GetActorDisplayName(Actor, DisplayNameStorage);

	const bool bActorDetailsSelected = Selection.IsSelected(Actor) && !Selection.IsComponentDetailsSelected();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bActorDetailsSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!Actor->GetRootComponent() && Actor->GetComponents().empty())
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}

	ImGui::PushID(Actor);
	const bool bOpen = ImGui::TreeNodeEx("##ActorNode", Flags, "%s", DisplayName);
	if (ImGui::IsItemClicked())
	{
		if (ImGui::GetIO().KeyShift)
		{
			Selection.SelectRange(Actor, Actors);
		}
		else if (ImGui::GetIO().KeyCtrl)
		{
			Selection.ToggleSelect(Actor);
		}
		else
		{
			Selection.SelectActorDetails(Actor);
		}
	}

	if (bOpen)
	{
		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			RenderSceneComponentNode(Root, Selection);
		}
		RenderNonSceneComponents(Actor, Selection);
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FEditorSceneWidget::RenderSceneComponentNode(USceneComponent* Comp, FSelectionManager& Selection)
{
	if (!IsValid(Comp) || ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents))
	{
		return;
	}

	FString Name;
	GetComponentDisplayName(Comp, Name);

	const auto& Children = Comp->GetChildren();
	bool bHasVisibleChildren = false;
	for (USceneComponent* Child : Children)
	{
		if (Child && !ShouldHideInComponentTree(Child, bShowEditorOnlyComponents))
		{
			bHasVisibleChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasVisibleChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (Selection.GetSelectedActorComponent() == Comp)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	const bool bIsRoot = Comp->GetParent() == nullptr;
	const bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetClass()->GetName());

	if (ImGui::IsItemClicked())
	{
		Selection.SelectActorComponent(Comp);
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_COMPONENT_REPARENT", &Comp, sizeof(USceneComponent*));
		ImGui::Text("Reparent %s", Name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("SCENE_COMPONENT_REPARENT"))
		{
			USceneComponent* DraggedComp = *(USceneComponent**)Payload->Data;
			if (DraggedComp && DraggedComp != Comp)
			{
				bool bIsChildOfDragged = false;
				USceneComponent* Check = Comp;
				while (Check)
				{
					if (Check == DraggedComp)
					{
						bIsChildOfDragged = true;
						break;
					}
					Check = Check->GetParent();
				}

				if (!bIsChildOfDragged)
				{
					DraggedComp->SetParent(Comp);
					if (UGizmoComponent* Gizmo = Selection.GetGizmo())
					{
						Gizmo->UpdateGizmoTransform();
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			RenderSceneComponentNode(Child, Selection);
		}
		ImGui::TreePop();
	}
}

void FEditorSceneWidget::RenderNonSceneComponents(AActor* Actor, FSelectionManager& Selection)
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<UActorComponent*> NonSceneComponents;
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp || Comp->IsA<USceneComponent>()) continue;
		if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) continue;
		NonSceneComponents.push_back(Comp);
	}

	if (NonSceneComponents.empty())
	{
		return;
	}

	ImGuiTreeNodeFlags GroupFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (ImGui::TreeNodeEx("##ActorComponents", GroupFlags, "Components"))
	{
		for (UActorComponent* Comp : NonSceneComponents)
		{
			FString Name;
			const char* Label = GetComponentDisplayName(Comp, Name);

			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Selection.GetSelectedActorComponent() == Comp)
			{
				Flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::TreeNodeEx(Comp, Flags, "%s (%s)", Label, Comp->GetClass()->GetName());
			if (ImGui::IsItemClicked())
			{
				Selection.SelectActorComponent(Comp);
			}
		}
		ImGui::TreePop();
	}
}
