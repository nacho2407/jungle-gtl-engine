#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"

void FEditorViewportOverlayWidget::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	(void)DeltaTime;
	(void)ViewOutput;

	if (!EditorEngine)
	{
		return;
	}

	FEditorSettings& Settings = EditorEngine->GetSettings();

	// 뷰포트 우상단에 오버레이 배치
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
	ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
	ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);

	ImGui::End();
}
