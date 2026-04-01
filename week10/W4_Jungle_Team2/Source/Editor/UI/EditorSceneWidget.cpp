#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Engine/Core/Common.h"
#include "GameFramework/WorldContext.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Serialization/SceneSaveManager.h"

#include <filesystem>

#include "Core/ResourceManager.h"

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

void FEditorSceneWidget::NewScene()
{
	if (!EditorEngine)
	{
		return;
	}

	EditorEngine->NewScene();
	NewSceneNotificationTimer = common::constants::ImGui::NotificationTimer;
}

void FEditorSceneWidget::SaveScene()
{
	SaveSceneToFilePath(SceneName);
}

void FEditorSceneWidget::LoadScene()
{
	if (!EditorEngine || SceneFiles.empty() || SelectedSceneIndex < 0)
	{
		return;
	}

	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(SceneFiles[SelectedSceneIndex]) + FSceneSaveManager::SceneExtension);
	LoadSceneFromFilePath(FPaths::ToUtf8(ScenePath.wstring()));
}

void FEditorSceneWidget::SaveSceneToFilePath(const FString& FilePath)
{
	if (!EditorEngine)
	{
		return;
	}

	std::filesystem::path TargetPath = std::filesystem::path(FPaths::ToWide(FilePath));
	const bool bNameOnlySave = !TargetPath.has_parent_path() && TargetPath.root_path().empty();
	if (TargetPath.extension().empty())
	{
		TargetPath += FSceneSaveManager::SceneExtension;
	}

	const FString FinalSceneName = FPaths::ToUtf8(TargetPath.stem().wstring());
	strncpy_s(SceneName, IM_ARRAYSIZE(SceneName), FinalSceneName.c_str(), _TRUNCATE);

	FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
	if (Ctx)
	{
		FEditorCameraState CamState;
		if (const FViewportCamera* Cam = EditorEngine->GetCamera())
		{
			CamState.Location = Cam->GetLocation();
			CamState.Rotation = FRotator(Cam->GetRotation());
			CamState.FOV = Cam->GetFOV() * (180.f / 3.14159265358979f);
			CamState.NearClip = Cam->GetNearPlane();
			CamState.FarClip = Cam->GetFarPlane();
			CamState.bValid = true;
		}

		FSceneSaveManager::SaveSceneAsJSON(FinalSceneName, *Ctx, &CamState);

		const std::filesystem::path SavedPath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
			/ (FPaths::ToWide(FinalSceneName) + FSceneSaveManager::SceneExtension);

		if (!bNameOnlySave && SavedPath != TargetPath)
		{
			const std::filesystem::path ParentPath = TargetPath.parent_path();
			if (!ParentPath.empty())
			{
				std::error_code CreateDirEc;
				std::filesystem::create_directories(ParentPath, CreateDirEc);
			}

			std::error_code CopyEc;
			std::filesystem::copy_file(SavedPath, TargetPath, std::filesystem::copy_options::overwrite_existing, CopyEc);
		}
	}

	SceneSaveNotificationTimer = common::constants::ImGui::NotificationTimer;
	RefreshSceneFileList();
}

void FEditorSceneWidget::LoadSceneFromFilePath(const FString& FilePath)
{
	if (!EditorEngine)
	{
		return;
	}

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

	SceneLoadNotificationTimer = common::constants::ImGui::NotificationTimer;
}

void FEditorSceneWidget::RefreshSceneAndAssets()
{
	RefreshSceneFileList();
	FResourceManager::Get().RefreshFromAssetDirectory(FPaths::ToUtf8(FPaths::AssetDirectoryPath()));
}

void FEditorSceneWidget::Render(float DeltaTime)
{
	using namespace common::constants::ImGui;
	(void)DeltaTime;

	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Scene Manager");

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

        // 화면에 보이는 항목(DisplayStart ~ DisplayEnd) 범위만 화면에 출력합니다.
		ImGuiListClipper Clipper;
        Clipper.Begin(static_cast<int>(Actors.size()));

        while (Clipper.Step())
        {
            for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
            {
                AActor* Actor = Actors[i];
                if (!Actor) continue;

                FString ActorName = Actor->GetFName().ToString();
                if (ActorName.empty())
                {
                    ActorName = Actor->GetTypeInfo()->name;
                }

                ImGui::PushID(i); 

                bool bIsSelected = Selection.IsSelected(Actor);
                if (ImGui::Selectable(ActorName.c_str(), bIsSelected))
                {
                    if (ImGui::GetIO().KeyShift)
                    {
                        Selection.AddSelect(Actor);
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
                
                ImGui::PopID();
            }
        }
        Clipper.End();
		ImGui::EndChild();
	}

	ImGui::End();
}
