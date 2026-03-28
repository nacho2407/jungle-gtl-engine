#include "ObjViewerControlWidget.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/Viewport/ObjViewerViewportClient.h"
#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Viewport/ViewportCamera.h"
#include "ImGui/imgui.h"

void FObjViewerControlWidget::Render(float DeltaTime)
{
	if (!Engine) return;

	FViewportCamera* Camera = Engine->GetCamera();
	FObjViewerSettings& Settings = FObjViewerSettings::Get();
	if (!Camera) return;

	// 우측 상단 배치 및 기본 크기 설정
	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(Viewport->WorkPos.x + Viewport->WorkSize.x - 350.0f, Viewport->WorkPos.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 400.0f), ImGuiCond_FirstUseEver);

	// 같은 이름("ObjViewer Panel")을 사용하여 StatWidget과 창을 통합할 수 있다.
	if (ImGui::Begin("ObjViewer Panel")) 
	{
		if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FVector CamPos = Camera->GetLocation();
			ImGui::Text("[Position] X: %.2f, Y: %.2f, Z: %.2f", CamPos.X, CamPos.Y, CamPos.Z);
			
			ImGui::DragFloat("Panning Speed", &Settings.CameraMoveSensitivity, 0.01f, 0.1f, 1.0f, "%.3f");
			ImGui::DragFloat("Dolly Speed", &Settings.CameraForwardSpeed, 1.0f, 10.0f, 1000.0f, "%.0f");

			if (ImGui::Button("Reset Camera Position", ImVec2(-FLT_MIN, 0)))
			{
				Engine->GetViewportClient().ResetCamera();
			}
		}

		// TODO: 애니메이션, 텍스쳐, 조명 기능이 추가되면 이를 Settings에 저장해서 관리한다.
		if (ImGui::CollapsingHeader("Rendering Options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static bool bAnimation = true;
			static bool bTexture = true;
			static bool bLighting = true;

			ImGui::Checkbox("Animation", &bAnimation);
			ImGui::Checkbox("Texture", &bTexture);
			ImGui::Checkbox("Lighting", &bLighting);
		}

		if (ImGui::CollapsingHeader("View Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Grid", &Settings.ShowFlags.bGrid);
			ImGui::Checkbox("Show Axis", &Settings.ShowFlags.bAxis);

			bool bWireframe = (Settings.ViewMode == EViewMode::Wireframe);
			if (ImGui::Checkbox("Wireframe Mode", &bWireframe))
			{
				Settings.ViewMode = bWireframe ? EViewMode::Wireframe : EViewMode::Lit;
			}
		}
	}
	ImGui::End();
}
