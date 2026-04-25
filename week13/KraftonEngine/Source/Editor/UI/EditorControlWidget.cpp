#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	if (!ImGui::Begin("Control Panel"))
	{
		ImGui::End();
		return;
	}

	// 액터 배치 기능은 뷰포트 Place Actor 메뉴로 이동하고, Control Panel은 카메라 제어만 유지한다.
	UCameraComponent* Camera = EditorEngine->GetCamera();
	if (!Camera)
	{
		ImGui::End();
		return;
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

	FRotator CamRot = Camera->GetRelativeRotation();
	float CameraRotation[3] = { CamRot.Roll, CamRot.Pitch, CamRot.Yaw };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		Camera->SetRelativeRotation(FRotator(CameraRotation[1], CameraRotation[2], CamRot.Roll));
	}

	ImGui::End();
}
