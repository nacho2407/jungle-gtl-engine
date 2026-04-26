#include "Editor/UI/EditorProjectSettingsWidget.h"
#include "Editor/Settings/ProjectSettings.h"
#include "ImGui/imgui.h"

void EditorProjectSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(360, 200), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Project Settings", &bOpen))
	{
		ImGui::End();
		return;
	}

	FProjectSettings& PS = FProjectSettings::Get();

	if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Shadows", &PS.bShadows);
	}

	ImGui::End();
}
