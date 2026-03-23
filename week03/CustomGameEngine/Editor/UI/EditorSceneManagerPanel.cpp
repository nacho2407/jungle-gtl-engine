#include "EditorSceneManagerPanel.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{

}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{

}

void EditorSceneManagerPanel::Draw(Editor* editor, ImGuiWindowLayout& layout)
{
	//일단 매 프레임마다 씬을 가져옵니다. 씬이 바뀌는 경우 때문.
	SceneManager* sceneMgr = GetWorld().GetSceneManager();
	if (sceneMgr == nullptr)
	{
		return;
	}

	ActiveScene = sceneMgr->GetActiveScene();
	const TArray<UScene*>& loadedScenes = sceneMgr->GetLoadedScenes();
	UScene* permanentScene = sceneMgr->GetPermanentScene();

	ImGui::SetNextWindowPos(layout.PosXY, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(layout.SizeWH, ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Manager");

	if (ImGui::CollapsingHeader("Loaded Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int32 i = 0; i < (int32)loadedScenes.size(); ++i)
		{
			UScene* scene = loadedScenes[i];
			bool isSelected = (ActiveScene == scene);

			FString sceneLabel = scene->Name.GetDisplayName() + FString("##") + std::to_string(i);
			if (ImGui::Selectable(sceneLabel.c_str(), isSelected))
			{
				sceneMgr->SetActiveScene(scene);
				editor->SelectComponent(nullptr);
			}

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Unload Scene"))
				{
					sceneMgr->UnloadScene(scene);
				}
				ImGui::EndPopup();
			}
		}
	}

	if (ActiveScene == nullptr)
	{
		ImGui::Text("No active scene.");
		ImGui::End();
		return;
	}
	else
	{
		const FString activeSceneName = ActiveScene->Name.GetDisplayName();
		ImGui::Text("Active Scene: %s", activeSceneName.c_str());
		if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (USceneComponent* component : ActiveScene->SceneComponents)
			{
				if (component == nullptr) continue;

				bool isSelected = (editor->GetSelectedComponent() == component);
				FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					editor->SelectComponent(component);
				}
			}
		}
	}

	ImGui::Separator();

	if (permanentScene && ImGui::CollapsingHeader("Permanent Scene", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (USceneComponent* component : permanentScene->SceneComponents)
		{
			if (component == nullptr) continue;

			bool isSelected = (editor->GetSelectedComponent() == component);
			FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

			if (ImGui::Selectable(label.c_str(), isSelected))
			{
				editor->SelectComponent(component);
			}
		}
	}
	ImGui::End();
}
