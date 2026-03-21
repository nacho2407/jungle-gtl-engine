#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Serialization/SceneSaveManager.h"

#include <filesystem>

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	RefreshSceneFileList();
}

void FEditorSceneWidget::RefreshSceneFileList()
{
	SceneFiles = FSceneSaveManager::GetSceneFileList();
	if (SelectedSceneIndex >= static_cast<int32>(SceneFiles.size()))
	{
		SelectedSceneIndex = SceneFiles.empty() ? -1 : 0;
	}
}

void FEditorSceneWidget::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	using namespace common::constants::ImGui;

	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Scene Manager");

	// New Scene
	if (ImGui::Button("New Scene"))
	{
		EditorEngine->GetGizmo()->SetVisibility(false);
		ViewOutput.Object = nullptr;
		EditorEngine->NewScene();
		NewSceneNotificationTimer = NotificationTimer;
	}
	if (NewSceneNotificationTimer > 0.0f)
	{
		NewSceneNotificationTimer -= DeltaTime;
		ImGui::SameLine();
		ImGui::Text("New scene created");
	}

	SEPARATOR();

	// Save Scene
	ImGui::InputText("Scene Name", SceneName, IM_ARRAYSIZE(SceneName));

	if (ImGui::Button("Save Scene"))
	{
		FSceneSaveManager::SaveSceneAsJSON(SceneName, EditorEngine->GetScene());
		SceneSaveNotificationTimer = NotificationTimer;
		RefreshSceneFileList();
	}
	if (SceneSaveNotificationTimer > 0.0f)
	{
		SceneSaveNotificationTimer -= DeltaTime;
		ImGui::SameLine();
		ImGui::Text("Scene saved");
	}

	SEPARATOR();

	// Load Scene (combo box)
	if (ImGui::Button("Refresh"))
	{
		RefreshSceneFileList();
	}
	ImGui::SameLine();
	ImGui::Text("Scene Files (%d)", static_cast<int32>(SceneFiles.size()));

	if (!SceneFiles.empty())
	{
		auto SceneNameGetter = [](void* Data, int32 Idx) -> const char*
		{
			auto* Files = static_cast<TArray<FString>*>(Data);
			if (Idx < 0 || Idx >= static_cast<int32>(Files->size())) return nullptr;
			return (*Files)[Idx].c_str();
		};

		ImGui::Combo("Scene File", &SelectedSceneIndex, SceneNameGetter, &SceneFiles, static_cast<int32>(SceneFiles.size()));

		if (ImGui::Button("Load Scene") && SelectedSceneIndex >= 0)
		{
			std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
				/ (FPaths::ToWide(SceneFiles[SelectedSceneIndex]) + FSceneSaveManager::SceneExtension);
			FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

			EditorEngine->GetGizmo()->SetVisibility(false);
			EditorEngine->ClearScene();
			ViewOutput.Object = nullptr;
			FSceneSaveManager::LoadSceneFromJSON(FilePath, EditorEngine->GetScene());
			EditorEngine->ResetViewport();
			SceneLoadNotificationTimer = NotificationTimer;
		}
		if (SceneLoadNotificationTimer > 0.0f)
		{
			SceneLoadNotificationTimer -= DeltaTime;
			ImGui::SameLine();
			ImGui::Text("Scene loaded");
		}
	}
	else
	{
		ImGui::Text("No scene files found in %s", FPaths::ToUtf8(FSceneSaveManager::GetSceneDirectory()).c_str());
	}

	SEPARATOR();

	// Active Scene List
	ImGui::Text("Active Scenes (%d)", static_cast<int32>(EditorEngine->GetScene().size()));
	ImGui::Separator();

	TArray<UWorld*>& Scenes = EditorEngine->GetScene();
	uint32 CurrentWorld = EditorEngine->GetCurrentWorld();

	for (uint32 i = 0; i < static_cast<uint32>(Scenes.size()); i++)
	{
		FString Label = "Scene " + std::to_string(i);
		if (i == CurrentWorld)
		{
			Label += " (Active)";
		}

		bool bSelected = (i == CurrentWorld);
		if (ImGui::Selectable(Label.c_str(), bSelected))
		{
			EditorEngine->SetCurrentWorld(i);
		}
	}

	ImGui::End();
}
