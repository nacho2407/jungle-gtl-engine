#include "Editor/UI/EditorFXAAWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"

void FEditorFXAAWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(360.0f, 260.0f), ImGuiCond_Once);

	if (ImGui::Begin("FXAA Settings", &FEditorSettings::Get().UI.bFXAA))
	{
		FEditorSettings& Settings = EditorEngine->GetSettings();
		ImGui::DragFloat("Edge Threshold", &Settings.FXAAEdgeThreshold, 0.001f, 0.01f, 1.0f, "%.4f");
		ImGui::DragFloat("Edge Threshold Min", &Settings.FXAAEdgeThresholdMin, 0.001f, 0.001f, 1.0f, "%.4f");
		ImGui::DragFloat("Search Threshold", &Settings.FXAASearchThreshold, 0.001f, 0.01f, 1.0f, "%.4f");
		ImGui::DragFloat("Subpix Trim", &Settings.FXAASubpixTrim, 0.001f, 0.0f, 1.0f, "%.4f");
		ImGui::DragFloat("Subpix Cap", &Settings.FXAASubpixCap, 0.001f, 0.0f, 1.0f, "%.4f");
		ImGui::Checkbox("Subpix Filtering", &Settings.bFXAASubpix);
		ImGui::SliderInt("Search Steps", &Settings.FXAASearchSteps, 1, 32);
	}
	ImGui::End();
}
