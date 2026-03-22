#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"

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
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
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

	if (PrimaryActor->GetRootComponent())
	{
		SEPARATOR();
		ImGui::Text("Transform (Primary)");
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
