#include "EditorSceneManagerPanel.h"

EditorSceneManagerPanel::EditorSceneManagerPanel()
{

}

EditorSceneManagerPanel::~EditorSceneManagerPanel()
{

}

void EditorSceneManagerPanel::Draw(Editor* editor)
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

	ImGui::Begin("Scene Manager");

	if (ImGui::CollapsingHeader("Loaded Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
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
			//if (ImGui::BeginPopupContextItem())
			//{
			//	if (ImGui::MenuItem("Unload Scene"))
			//	{
			//		sceneMgr->UnloadScene(scene);
			//	}
			//	ImGui::EndPopup();
			//}
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

				bool isSelected = (editor->IsComponentSelected(component));
				FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

				//isSelected에 따라 선택된 컴포넌트는 다른 색으로 표시됩니다. 클릭 시 해당 컴포넌트를 선택합니다.
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					//bool bIsCtrlDown = InputManager::GetInstance().IsKeyDown(VK_CONTROL);
					//ImGUI에서 컨트롤 키 입력을 감지하는 방식이며 InputManager를 사용하면 작동하지 않습니다.
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

	if (permanentScene && ImGui::CollapsingHeader("Permanent Scene", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (USceneComponent* component : permanentScene->SceneComponents)
		{
			if (component == nullptr) continue;

			bool isSelected = editor->IsComponentSelected(component);
			FString label = component->Name.GetDisplayName() + " (UUID: " + std::to_string(component->UUID) + ")";

			if (ImGui::Selectable(label.c_str(), isSelected))
			{
				editor->SelectSingleComponent(component);
			}
		}
	}
	ImGui::End();
}
