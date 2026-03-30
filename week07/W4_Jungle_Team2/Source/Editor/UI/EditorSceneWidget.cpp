#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Common.h"
#include "GameFramework/WorldContext.h"

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

void FEditorSceneWidget::Render(float DeltaTime)
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
		FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
		if (Ctx)
		{
			// Perspective 카메라(0번) 상태를 씬에 함께 저장
			FEditorCameraState CamState;
			if (const FViewportCamera* Cam = EditorEngine->GetCamera())
			{
				CamState.Location = Cam->GetLocation();
				CamState.Rotation = FRotator(Cam->GetRotation());
				CamState.FOV      = Cam->GetFOV() * (180.f / 3.14159265358979f);
				CamState.NearClip = Cam->GetNearPlane();
				CamState.FarClip  = Cam->GetFarPlane();
				CamState.bValid   = true;
			}
			FSceneSaveManager::SaveSceneAsJSON(SceneName, *Ctx, &CamState);
		}
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

			EditorEngine->ClearScene();
			FWorldContext LoadCtx;
			FEditorCameraState LoadedCam;
			FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, &LoadedCam);
			if (LoadCtx.World)
			{
				EditorEngine->GetWorldList().push_back(LoadCtx);
				EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
			}
			EditorEngine->ResetViewport();

			// ResetViewport 가 카메라를 InitViewPos 로 초기화하므로 그 이후에 덮어씁니다
			if (LoadedCam.bValid)
			{
				if (FViewportCamera* Cam = EditorEngine->GetCamera())
				{
					Cam->SetLocation(LoadedCam.Location);
					Cam->SetRotation(FQuat(LoadedCam.Rotation));
					Cam->SetFOV(LoadedCam.FOV * (3.14159265358979f / 180.f));
					Cam->SetNearPlane(LoadedCam.NearClip);
					Cam->SetFarPlane(LoadedCam.FarClip);
				}
			}
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

	// Actor Outliner
	UWorld* World = EditorEngine->GetWorld();
	if (World)
	{
		const TArray<AActor*>& Actors = World->GetActors();
		ImGui::Text("Actors (%d)", static_cast<int32>(Actors.size()));
		ImGui::Separator();

		// Fill remaining space with scrollable child region
		FSelectionManager& Selection = EditorEngine->GetSelectionManager();

		ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);
		for (AActor* Actor : Actors)
		{
			if (!Actor) continue;

			FString ActorName = Actor->GetFName().ToString();
			if (ActorName.empty())
			{
				ActorName = Actor->GetTypeInfo()->name;
			}

			bool bIsSelected = Selection.IsSelected(Actor);
			if (ImGui::Selectable(ActorName.c_str(), bIsSelected))
			{
				if (ImGui::GetIO().KeyShift)
				{
					Selection.SelectRange(Actor, Actors);
				}
				else if (ImGui::GetIO().KeyCtrl)
				{
					Selection.ToggleSelect(Actor);
				}
				else
				{
					Selection.Select(Actor);
				}
			}
		}
		ImGui::EndChild();
	}

	ImGui::End();
}
