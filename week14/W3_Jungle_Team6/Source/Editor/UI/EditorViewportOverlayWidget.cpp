#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"

void FEditorViewportOverlayWidget::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	(void)DeltaTime;
	(void)ViewOutput;

	FEditorSettings& Settings = FEditorSettings::Get();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const float Padding = 10.0f;
	ImVec2 WindowPos(Viewport->WorkPos.x + Viewport->WorkSize.x - Padding, Viewport->WorkPos.y + Padding);

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.6f);

	ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove;

	if (!ImGui::Begin("##ViewportOverlay", nullptr, Flags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(bExpanded ? "Settings <<" : "Settings >>"))
	{
		bExpanded = !bExpanded;
	}

	if (bExpanded)
	{
		ImGui::Separator();

		// View Mode
		ImGui::Text("View Mode");
		int32 CurrentMode = static_cast<int32>(Settings.ViewMode);
		ImGui::RadioButton("Lit", &CurrentMode, static_cast<int32>(EViewMode::Lit));
		ImGui::SameLine();
		ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
		ImGui::SameLine();
		ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
		Settings.ViewMode = static_cast<EViewMode>(CurrentMode);

		ImGui::Separator();

		// Show Flags
		ImGui::Text("Show");
		ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
		ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
		ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
		ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);

		ImGui::Separator();

		// Grid Settings
		ImGui::Text("Grid");
		ImGui::SliderFloat("Spacing", &Settings.GridSpacing, 0.1f, 10.0f, "%.1f");
		ImGui::SliderInt("Half Line Count", &Settings.GridHalfLineCount, 10, 500);

		ImGui::Separator();

		// Camera Sensitivity
		ImGui::Text("Camera");
		ImGui::SliderFloat("Move Sensitivity", &Settings.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("Rotate Sensitivity", &Settings.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");
	}

	ImGui::End();
}
