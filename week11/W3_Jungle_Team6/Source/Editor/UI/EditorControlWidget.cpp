#include "Editor/UI/EditorControlWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BillboardComponent.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorControlWidget::Initialize(FEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectedPrimitiveType = static_cast<int32>(EPrimitiveType::EPT_Cube);
}

void FEditorControlWidget::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	// Stats
	ImGui::Text("FPS : %.1f", EditorEngine->GetMainLoopFPS());
	ImGui::SameLine();
	ImGui::Text("Memory Allocated : %d", EngineStatics::GetTotalAllocationBytes());
	ImGui::SameLine();
	ImGui::Text("Times Allocated : %d", EngineStatics::GetTotalAllocationCount());

	SEPARATOR();

	// Spawn
	ImGui::Combo("Primitive", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

	if (ImGui::Button("Spawn"))
	{
		for (int32 i = 0; i < NumberOfSpawnedActors; i++)
		{
			switch (static_cast<EPrimitiveType>(SelectedPrimitiveType))
			{
			case EPrimitiveType::EPT_Cube:
				EditorEngine->SpawnNewPrimitiveActor<UCubeComponent>(CurSpawnPoint);
				break;
			case EPrimitiveType::EPT_Sphere:
				EditorEngine->SpawnNewPrimitiveActor<USphereComponent>(CurSpawnPoint);
				break;
			case EPrimitiveType::EPT_Plane:
				EditorEngine->SpawnNewPrimitiveActor<UPlaneComponent>(CurSpawnPoint);
				break;
			case EPrimitiveType::EPT_Quad:
				EditorEngine->SpawnNewPrimitiveActor <UBillboardComponent>(CurSpawnPoint);
				break;
			}
		}
		NumberOfSpawnedActors = 1;
	}
	ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

	SEPARATOR();

	// Camera
	FCameraState& CameraState = EditorEngine->GetCameraState();
	ImGui::Checkbox("Orthographic", &(CameraState.bIsOrthogonal));

	float CameraFOV_Deg = CameraState.FOV * RAD_TO_DEG;
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		CameraState.FOV = CameraFOV_Deg * DEG_TO_RAD;
	}

	float OrthoWidth = CameraState.OrthoWidth;
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		CameraState.OrthoWidth = Clamp(OrthoWidth, 0.1f, 1000.0f);
	}

	UCamera* Camera = EditorEngine->GetCamera();
	FVector CamPos = Camera->GetWorldLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
	}

	FVector CamRot = Camera->GetRelativeRotation();
	float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		Camera->SetRelativeRotation(FVector(Clamp(CameraRotation[0], CamRot.X, CamRot.X), CameraRotation[1], CameraRotation[2]));
	}

	SEPARATOR();

	// Gizmo Space / Mode
	static int32 SelectedSpace = 0;
	if (ImGui::RadioButton("World", &SelectedSpace, 0))
	{
		EditorEngine->GetGizmo()->SetWorldSpace(true);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Local", &SelectedSpace, 1))
	{
		EditorEngine->GetGizmo()->SetWorldSpace(false);
	}

	SEPARATOR();

	if (ImGui::Button("Translate")) EditorEngine->GetGizmo()->SetTranslateMode();
	ImGui::SameLine();
	if (ImGui::Button("Rotate")) EditorEngine->GetGizmo()->SetRotateMode();
	ImGui::SameLine();
	if (ImGui::Button("Scale")) EditorEngine->GetGizmo()->SetScaleMode();

	ImGui::End();
}
