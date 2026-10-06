#include "Editor/UI/Panel/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Component/ActorComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Physics/PhysicalAnimationComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cstring>

namespace
{
	struct FComponentClassGroup
	{
		const char* Label = nullptr;
		UClass* AnchorClass = nullptr;
		TArray<UClass*> Classes;
	};

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

	void AddComponentClassGroup(TArray<FComponentClassGroup>& Groups, const char* Label, UClass* AnchorClass)
	{
		FComponentClassGroup Group;
		Group.Label = Label;
		Group.AnchorClass = AnchorClass;
		Groups.push_back(Group);
	}

	UClass* FindComponentClassGroupAnchor(UClass* ComponentClass, const TArray<FComponentClassGroup>& Groups)
	{
		if (!ComponentClass)
		{
			return nullptr;
		}

		for (const FComponentClassGroup& Group : Groups)
		{
			if (Group.AnchorClass && ComponentClass->IsA(Group.AnchorClass))
			{
				return Group.AnchorClass;
			}
		}

		return nullptr;
	}

	void BuildComponentClassGroups(TArray<FComponentClassGroup>& OutGroups, TArray<UClass*>& OutOtherClasses)
	{
		OutGroups.clear();
		OutOtherClasses.clear();

		TArray<UClass*> ComponentClasses;
		for (UClass* Cls : UClass::GetAllClasses())
		{
			if (Cls && Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_HiddenInComponentList))
			{
				ComponentClasses.push_back(Cls);
			}
		}

		std::sort(ComponentClasses.begin(), ComponentClasses.end(),
			[](const UClass* A, const UClass* B)
			{
				return std::strcmp(A->GetName(), B->GetName()) < 0;
			});

		AddComponentClassGroup(OutGroups, "Light", ULightComponentBase::StaticClass());
		AddComponentClassGroup(OutGroups, "Movement", UMovementComponent::StaticClass());
		AddComponentClassGroup(OutGroups, "Physics", UPhysicalAnimationComponent::StaticClass());
		AddComponentClassGroup(OutGroups, "Primitive", UPrimitiveComponent::StaticClass());

		for (UClass* Cls : ComponentClasses)
		{
			UClass* AnchorClass = FindComponentClassGroupAnchor(Cls, OutGroups);
			if (!AnchorClass)
			{
				OutOtherClasses.push_back(Cls);
				continue;
			}

			for (FComponentClassGroup& Group : OutGroups)
			{
				if (Group.AnchorClass == AnchorClass)
				{
					Group.Classes.push_back(Cls);
					break;
				}
			}
		}
	}

	bool IsComponentInSceneSubtree(USceneComponent* Root, UActorComponent* Component)
	{
		if (!IsValid(Root) || !IsValid(Component))
		{
			return false;
		}

		if (Root == Component)
		{
			return true;
		}

		for (USceneComponent* Child : Root->GetChildren())
		{
			if (IsComponentInSceneSubtree(Child, Component))
			{
				return true;
			}
		}

		return false;
	}

	bool DoesRemoveAffectSelection(UActorComponent* Component, UActorComponent* SelectedComponent)
	{
		if (!IsValid(Component) || !IsValid(SelectedComponent))
		{
			return false;
		}

		if (Component == SelectedComponent)
		{
			return true;
		}

		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			return IsComponentInSceneSubtree(SceneComponent, SelectedComponent);
		}

		return false;
	}
}

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorSceneWidget::Render(const FEditorPanelContext& Context)
{
	(void)Context;

	if (!EditorEngine)
	{
		return;
	}

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();

	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Scene Manager");

	// 씬 파일 작업은 상단 메뉴로 옮기고, Scene Manager는 액터 목록만 유지한다.
	RenderActorOutliner(Selection);

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
	RenderActorContextMenu(Actor, Selection);

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
	RenderComponentContextMenu(Comp, Selection);

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
		RenderComponentContextMenu(Comp, Selection);
	}
}

void FEditorSceneWidget::RenderActorContextMenu(AActor* Actor, FSelectionManager& Selection)
{
	if (!IsValid(Actor))
	{
		return;
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::BeginMenu("Add Component"))
		{
			RenderAddComponentMenu(Actor, nullptr, Selection);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete Actor"))
		{
			Selection.Select(Actor);
			Selection.DeleteSelectedActors();
		}
		ImGui::EndPopup();
	}
}

void FEditorSceneWidget::RenderComponentContextMenu(UActorComponent* Component, FSelectionManager& Selection)
{
	if (!IsValid(Component))
	{
		return;
	}

	AActor* Actor = Component->GetOwner();
	if (!IsValid(Actor))
	{
		return;
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (ImGui::BeginMenu("Add Child Component"))
			{
				RenderAddComponentMenu(Actor, SceneComponent, Selection);
				ImGui::EndMenu();
			}
			ImGui::Separator();
		}

		if (ImGui::MenuItem("Delete Component"))
		{
			RemoveComponentFromActor(Component, Selection);
		}

		ImGui::EndPopup();
	}
}

void FEditorSceneWidget::RenderAddComponentMenu(AActor* Actor, USceneComponent* AttachParent, FSelectionManager& Selection)
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<FComponentClassGroup> ComponentGroups;
	TArray<UClass*> OtherClasses;
	BuildComponentClassGroups(ComponentGroups, OtherClasses);

	auto AddComponentClassItem = [&](UClass* Cls)
	{
		if (ImGui::MenuItem(Cls->GetName()))
		{
			AddComponentToActor(Actor, Cls, AttachParent, Selection);
			ImGui::CloseCurrentPopup();
		}
	};

	for (const FComponentClassGroup& Group : ComponentGroups)
	{
		if (Group.Classes.empty())
		{
			continue;
		}

		if (ImGui::BeginMenu(Group.Label))
		{
			for (UClass* Cls : Group.Classes)
			{
				AddComponentClassItem(Cls);
			}
			ImGui::EndMenu();
		}
	}

	if (!OtherClasses.empty() && ImGui::BeginMenu("Other"))
	{
		for (UClass* Cls : OtherClasses)
		{
			AddComponentClassItem(Cls);
		}
		ImGui::EndMenu();
	}
}

UActorComponent* FEditorSceneWidget::AddComponentToActor(
	AActor* Actor,
	UClass* ComponentClass,
	USceneComponent* AttachParent,
	FSelectionManager& Selection)
{
	if (!IsValid(Actor) || !ComponentClass)
	{
		return nullptr;
	}

	UActorComponent* Component = Actor->AddComponentByClass(ComponentClass);
	if (!IsValid(Component))
	{
		return nullptr;
	}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		if (IsValid(AttachParent))
		{
			SceneComponent->AttachToComponent(AttachParent);
		}
		else if (USceneComponent* Root = Actor->GetRootComponent())
		{
			if (Root != SceneComponent)
			{
				SceneComponent->AttachToComponent(Root);
			}
		}
		else
		{
			Actor->SetRootComponent(SceneComponent);
		}

		if (Component->IsA<ULightComponentBase>())
		{
			Cast<ULightComponentBase>(Component)->EnsureEditorBillboard();
		}
		else if (Component->IsA<UDecalComponent>())
		{
			Cast<UDecalComponent>(Component)->EnsureEditorBillboard();
		}
		else if (Component->IsA<UHeightFogComponent>())
		{
			Cast<UHeightFogComponent>(Component)->EnsureEditorBillboard();
		}
	}

	Selection.SelectActorComponent(Component);
	if (UGizmoComponent* Gizmo = Selection.GetGizmo())
	{
		Gizmo->UpdateGizmoTransform();
	}
	return Component;
}

void FEditorSceneWidget::RemoveComponentFromActor(UActorComponent* Component, FSelectionManager& Selection)
{
	if (!IsValid(Component))
	{
		return;
	}

	AActor* Actor = Component->GetOwner();
	if (!IsValid(Actor))
	{
		return;
	}

	UActorComponent* SelectedComponent = Selection.GetSelectedActorComponent();
	const bool bAffectsSelection = DoesRemoveAffectSelection(Component, SelectedComponent);
	Actor->RemoveComponent(Component);

	if (bAffectsSelection)
	{
		Selection.SelectActorDetails(Actor);
	}
	else if (UGizmoComponent* Gizmo = Selection.GetGizmo())
	{
		Gizmo->UpdateGizmoTransform();
	}
}
