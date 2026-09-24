#include "EditorSceneManagerPanel.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{
}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{
}

void EditorSceneManagerPanel::Draw(Editor* editor)
{
	SceneManager* sceneMgr = GetWorld().GetSceneManager();
	if (sceneMgr == nullptr)
	{
		return;
	}

	ActiveScene = sceneMgr->GetActiveScene();
	const TArray<UScene*>& loadedScenes = sceneMgr->GetLoadedScenes();
	UScene* permanentScene = sceneMgr->GetPermanentScene();

	ImGui::Begin("Scene Manager");

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImGui::GetIO().KeyCtrl &&
		ImGui::IsKeyPressed(ImGuiKey_A, false))
	{
		TArray<USceneComponent*> componentsToSelect;
		if (ActiveScene != nullptr)
		{
			for (USceneComponent* component : ActiveScene->SceneComponents)
			{
				if (component != nullptr)
				{
					componentsToSelect.push_back(component);
				}
			}
		}
		editor->SelectAllComponents(componentsToSelect);
	}

	if (ImGui::CollapsingHeader("Loaded Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bSceneRemoved = false;
		for (int32 i = 0; i < (int32)loadedScenes.size(); ++i)
		{
			UScene* scene = loadedScenes[i];
			bool isSelected = (ActiveScene == scene);

			FString sceneLabel = scene->Name.GetDisplayName();
			if (ImGui::Selectable(sceneLabel.c_str(), isSelected))
			{
				sceneMgr->SetActiveScene(scene);
				editor->ClearSelection();
			}
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Scene"))
				{
					editor->ClearSelection();
					sceneMgr->UnloadScene(scene);
					ActiveScene = sceneMgr->GetActiveScene();
					bSceneRemoved = true;
				}
				ImGui::EndPopup();
			}

			if (bSceneRemoved)
			{
				break;
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
		if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (USceneComponent* component : ActiveScene->SceneComponents)
			{
				if (component == nullptr) continue;

				bool isSelected = (editor->IsComponentSelected(component));
				FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					bool bIsCtrlDown = ImGui::GetIO().KeyCtrl;
					if (!bIsCtrlDown)
					{
						editor->SelectSingleComponent(component);
					}
					else
					{
						editor->SelectMultiComponent(component);
					}
				}
			}
		}
	}

	ImGui::Separator();

	//if (permanentScene && ImGui::CollapsingHeader("Permanent Scene", ImGuiTreeNodeFlags_DefaultOpen))
	//{
	//	for (USceneComponent* component : permanentScene->SceneComponents)
	//	{
	//		if (component == nullptr) continue;

	//		bool isSelected = editor->IsComponentSelected(component);
	//		FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

	//		if (ImGui::Selectable(label.c_str(), isSelected))
	//		{
	//			editor->SelectSingleComponent(component);
	//		}
	//	}
	//}
	ImGui::End();
}
