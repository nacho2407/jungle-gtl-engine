#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/PrimitiveComponent.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorPropertyWidget::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 300.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Property Window");

	UObject* ObjectPicked = ViewOutput.Object;
	if (!ObjectPicked)
	{
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	ImGui::Text("Class: %s", ObjectPicked->GetTypeInfo()->name);
	ImGui::Text("Object Size: %d", sizeof(*ObjectPicked));

	if (ObjectPicked->IsA<USceneComponent>())
	{
		SEPARATOR();
		ImGui::Text("Transform");
		ImGui::Separator();

		USceneComponent* SceneComp = ObjectPicked->Cast<USceneComponent>();
		FVector Pos = SceneComp->GetWorldLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		FVector Rot = SceneComp->GetRelativeRotation();
		float RotArray[3] = { Rot.X, Rot.Y, Rot.Z };

		FVector Scale = SceneComp->GetRelativeScale();
		float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };

		UGizmoComponent* Gizmo = EditorEngine->GetGizmo();
		if (ImGui::DragFloat3("Location", PosArray, 0.1f))
		{
			Gizmo->SetTargetLocation(FVector(PosArray[0], PosArray[1], PosArray[2]));
		}
		if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
		{
			Gizmo->SetTargetRotation(FVector(RotArray[0], RotArray[1], RotArray[2]));
		}
		if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
		{
			Gizmo->SetTargetScale(FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]));
		}

		SEPARATOR();

		if (ImGui::Button("Remove Object"))
		{
			if (ObjectPicked->IsA<USceneComponent>())
			{
				USceneComponent* Comp = ObjectPicked->Cast<USceneComponent>();
				if (Comp->GetOwner())
				{
					UObjectManager::Get().DestroyObject(Comp->GetOwner());
				}
			}
			ObjectPicked->bPendingKill = true;
			EditorEngine->GetGizmo()->SetVisibility(false);
			EditorEngine->GetGizmo()->Deactivate();
			ViewOutput.Object = nullptr;
		}
	}

	ImGui::End();
}
