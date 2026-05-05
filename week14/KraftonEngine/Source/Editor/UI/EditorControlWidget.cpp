#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"

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
	// D.3: ViewTransform 이 SoT — Camera 컴포넌트 setter 직접 호출 안 함.
	FLevelEditorViewportClient* VC = EditorEngine->GetActiveViewport();
	if (!VC)
	{
		ImGui::End();
		return;
	}

	FViewportCameraTransform& VT = VC->GetViewTransform();
	bool bChanged = false;

	float CameraFOV_Deg = VT.FOV * RAD_TO_DEG;
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		VT.FOV = CameraFOV_Deg * DEG_TO_RAD;
		bChanged = true;
	}

	float OrthoWidth = VT.OrthoZoom;
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		VT.OrthoZoom = Clamp(OrthoWidth, 0.1f, 1000.0f);
		bChanged = true;
	}

	float CameraLocation[3] = { VT.ViewLocation.X, VT.ViewLocation.Y, VT.ViewLocation.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		VT.ViewLocation = FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]);
		bChanged = true;
	}

	float CameraRotation[3] = { VT.ViewRotation.Roll, VT.ViewRotation.Pitch, VT.ViewRotation.Yaw };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		VT.ViewRotation = FRotator(CameraRotation[1], CameraRotation[2], CameraRotation[0]);
		bChanged = true;
	}

	if (bChanged)
	{
		VC->NotifyViewTransformChanged();
	}

	ImGui::End();
}
