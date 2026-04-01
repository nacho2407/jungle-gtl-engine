#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/StaticMeshActor.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorControlWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectedPrimitiveType = 0;
}

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	// Spawn
	ImGui::Combo("Primitive", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

	if (ImGui::Button("Spawn"))
	{
		UWorld* World = EditorEngine->GetWorld();
		for (int32 i = 0; i < NumberOfSpawnedActors; i++)
		{
			switch (SelectedPrimitiveType)
			{
			case 0: // Cube
			{
				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				Actor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
				break;
			}
			case 1: // Sphere
			{
				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				Actor->InitDefaultComponents("Data/BasicShape/Sphere.OBJ");
				break;
			}
			}
		}
		NumberOfSpawnedActors = 1;
	}
	ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

	SEPARATOR();

	// Camera
	UCameraComponent* Camera = EditorEngine->GetCamera();
	bool bIsOrtho = Camera->IsOrthogonal();
	if (ImGui::Checkbox("Orthographic", &bIsOrtho))
	{
		Camera->SetOrthographic(bIsOrtho);
	}

	float CameraFOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		Camera->SetFOV(CameraFOV_Deg * DEG_TO_RAD);
	}

	float OrthoWidth = Camera->GetOrthoWidth();
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
	}

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

	ImGui::End();
}
