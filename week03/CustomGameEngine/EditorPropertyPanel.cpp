#include "EditorPropertyPanel.h"
#include "ImGui/imgui.h"
#include "USceneComponent.h"

EditorPropertyPanel::EditorPropertyPanel()
{
}

EditorPropertyPanel::~EditorPropertyPanel()
{
}

void EditorPropertyPanel::Draw(USceneComponent* selectedComponent)
{
	ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Property Panel");

	if (selectedComponent)
	{
		selectedComponent->DrawProperties();
	}

	ImGui::End();
}
