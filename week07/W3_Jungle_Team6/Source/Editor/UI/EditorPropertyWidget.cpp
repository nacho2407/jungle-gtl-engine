#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorPropertyWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 300.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Property Window");

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	AActor* PrimaryActor = Selection.GetPrimarySelection();
	if (!PrimaryActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	// Actor 선택이 바뀌면 컴포넌트 선택 초기화
	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = PrimaryActor;
	}

	const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
	const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

	if (SelectionCount > 1)
	{
		ImGui::Text("%d objects selected", SelectionCount);
		ImGui::Separator();
		for (AActor* Actor : SelectedActors)
		{
			if (!Actor) continue;
			FString Name = Actor->GetFName().ToString();
			if (Name.empty()) Name = Actor->GetTypeInfo()->name;
			ImGui::BulletText("%s", Name.c_str());
		}
	}
	else
	{
		ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);
		ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());
	}

	// ---- Component Tree ----
	SEPARATOR();
	RenderComponentTree(PrimaryActor);

	// ---- Selected Component Properties ----
	if (SelectedComponent)
	{
		SEPARATOR();
		RenderComponentProperties();
	}

	// ---- Actor Transform (multi-select support) ----
	if (PrimaryActor->GetRootComponent())
	{
		SEPARATOR();
		ImGui::Text("Actor Transform");
		ImGui::Separator();

		FVector Pos = PrimaryActor->GetActorLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		FVector Rot = PrimaryActor->GetActorRotation();
		float RotArray[3] = { Rot.X, Rot.Y, Rot.Z };

		FVector Scale = PrimaryActor->GetActorScale();
		float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };

		if (ImGui::DragFloat3("Location", PosArray, 0.1f))
		{
			FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->AddActorWorldOffset(Delta);
			}
			EditorEngine->GetGizmo()->UpdateGizmoTransform();
		}
		if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
		{
			FVector Delta = FVector(RotArray[0], RotArray[1], RotArray[2]) - Rot;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->SetActorRotation(Actor->GetActorRotation() + Delta);
			}
			EditorEngine->GetGizmo()->UpdateGizmoTransform();
		}
		if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
		{
			FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->SetActorScale(Actor->GetActorScale() + Delta);
			}
		}

		SEPARATOR();

		if (SelectionCount > 1)
		{
			char RemoveLabel[64];
			snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
			if (ImGui::Button(RemoveLabel))
			{
				for (AActor* Actor : SelectedActors)
				{
					if (Actor && Actor->GetWorld())
					{
						Actor->GetWorld()->DestroyActor(Actor);
					}
				}
				Selection.ClearSelection();
			}
		}
		else
		{
			if (ImGui::Button("Remove Object"))
			{
				if (PrimaryActor->GetWorld())
				{
					PrimaryActor->GetWorld()->DestroyActor(PrimaryActor);
				}
				Selection.ClearSelection();
			}
		}
	}

	ImGui::End();
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	ImGui::Text("Components");
	ImGui::Separator();

	USceneComponent* Root = Actor->GetRootComponent();

	// SceneComponent 트리: RootComponent부터 재귀
	if (Root)
	{
		RenderSceneComponentNode(Root);
	}

	// Non-scene ActorComponents: 트리 하단에 flat 나열
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue; // SceneComponent는 트리에서 이미 표시

		FString Name = Comp->GetFName().ToString();
		if (Name.empty()) Name = Comp->GetTypeInfo()->name;

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (SelectedComponent == Comp)
			Flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx(Comp, Flags, "[%s] %s", Comp->GetTypeInfo()->name, Name.c_str());
		if (ImGui::IsItemClicked())
		{
			SelectedComponent = Comp;
		}
	}
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent* Comp)
{
	if (!Comp) return;

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetTypeInfo()->name;

	const auto& Children = Comp->GetChildren();
	bool bHasChildren = !Children.empty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren)
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (SelectedComponent == Comp)
		Flags |= ImGuiTreeNodeFlags_Selected;

	// Root인 경우 표시
	bool bIsRoot = (Comp->GetParent() == nullptr);
	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetTypeInfo()->name
	);

	if (ImGui::IsItemClicked())
	{
		SelectedComponent = Comp;
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			RenderSceneComponentNode(Child);
		}
		ImGui::TreePop();
	}
}

void FEditorPropertyWidget::RenderComponentProperties()
{
	ImGui::Text("Component: %s", SelectedComponent->GetTypeInfo()->name);
	ImGui::Text("Name: %s", SelectedComponent->GetFName().ToString().c_str());

	// SceneComponent: Transform 편집
	if (SelectedComponent->IsA<USceneComponent>())
	{
		USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
		ImGui::Separator();

		FVector Loc = SceneComp->GetRelativeLocation();
		float LocArr[3] = { Loc.X, Loc.Y, Loc.Z };
		if (ImGui::DragFloat3("Comp Location", LocArr, 0.1f))
		{
			SceneComp->SetRelativeLocation(FVector(LocArr[0], LocArr[1], LocArr[2]));
		}

		FVector Rot = SceneComp->GetRelativeRotation();
		float RotArr[3] = { Rot.X, Rot.Y, Rot.Z };
		if (ImGui::DragFloat3("Comp Rotation", RotArr, 0.1f))
		{
			SceneComp->SetRelativeRotation(FVector(RotArr[0], RotArr[1], RotArr[2]));
		}

		FVector Scl = SceneComp->GetRelativeScale();
		float SclArr[3] = { Scl.X, Scl.Y, Scl.Z };
		if (ImGui::DragFloat3("Comp Scale", SclArr, 0.1f))
		{
			SceneComp->SetRelativeScale(FVector(SclArr[0], SclArr[1], SclArr[2]));
		}
	}

	// UActorComponent 공통: Active 상태
	ImGui::Separator();
	bool bActive = SelectedComponent->IsActive();
	if (ImGui::Checkbox("Active", &bActive))
	{
		SelectedComponent->SetActive(bActive);
	}
}
