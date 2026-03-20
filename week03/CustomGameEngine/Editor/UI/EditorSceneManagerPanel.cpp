#include "EditorSceneManagerPanel.h"
#include "ImGui/imgui.h"
#include "World.h"
#include "Editor/Editor.h"
#include "Scene.h"
#include "Component/SceneComponent.h"
#include "SceneManager.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{

}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{

}

void EditorSceneManagerPanel::Draw(Editor* editor)
{
	//일단 매 프레임마다 씬을 가져옵니다. 씬이 바뀌는 경우 때문.
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
	ImGui::Text("NAME");
	ImGui::SameLine(0.0f, 90.0f);
	ImGui::Text("UUID");

	for (USceneComponent* component : ActiveScene->SceneComponents)
	{
		if (component == nullptr)
		{
			continue;
		}
		bool isSelected = (editor->GetSelectedComponent() == component);
		FString	displayName = component->Name.GetDisplayName();
		if (ImGui::Selectable(displayName.c_str(), isSelected))
		{
			editor->SelectComponent(component);
		}
		if (isSelected)
		{
			//마우스 없이도 사용할 수 있게 도와주는 내비게이션용.
			ImGui::SetItemDefaultFocus();
		}
		ImGui::SameLine(0.0f, 20.0f);
		ImGui::Text("%u", component->UUID);
	}

	ImGui::End();
}