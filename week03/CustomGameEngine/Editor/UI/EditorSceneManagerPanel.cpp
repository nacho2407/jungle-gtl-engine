#include "EditorSceneManagerPanel.h"
#include "ImGui/imgui.h"
#include "World.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{
	ActiveScene = GetWorld().GetSceneManager()->GetActiveScene();
}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{

}

void EditorSceneManagerPanel::Draw()
{
	ImGui::SetNextWindowPos(ImVec2(1600, 315), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Manager");

	ImGui::End();
}