#include "EditorSceneManagerPanel.h"
#include "ImGui/imgui.h"
#include "World.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{
	/*TODO : 멀티 scene을 위해 추후에 ActiveScene을 바꾸는 기능 추가 필요*/
	///ActiveScene = GetWorld().GetSceneManager()->GetActiveScene();
}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{

}

void EditorSceneManagerPanel::Draw()
{
	//일단 매 프레임마다 씬을 가져오지는 않습니다.
	ActiveScene = GetWorld().GetSceneManager()->GetActiveScene();

	ImGui::SetNextWindowPos(ImVec2(1600, 315), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Manager");

	if (ActiveScene == nullptr)
	{
		ImGui::Text("No active scene.");
		ImGui::End();
		return;
	}

	ImGui::Text("SceneComponents: %d", static_cast<int>(ActiveScene->SceneComponents.size()));
	ImGui::Separator();

	for (USceneComponent* component : ActiveScene->SceneComponents)
	{
		if (component == nullptr)
		{
			continue;
		}

		ImGui::Text("UUID: %u", component->UUID);
	}

	ImGui::End();
}