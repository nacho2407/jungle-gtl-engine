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

	EditorEngine->GetMainPanel().ResetWidgetSelections();
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

	EditorEngine->GetMainPanel().ResetWidgetSelections();
	EditorEngine->ClearScene();
	FWorldContext LoadCtx;
	FEditorCameraState LoadedCam;
	FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, &LoadedCam);
	if (LoadCtx.World)
	{
		EditorEngine->GetWorldList().push_back(LoadCtx);
		EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
		EditorEngine->ApplySpatialIndexMaintenanceSettings(LoadCtx.World);
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
			// ApplyCameraMode 가 설정한 TargetLocation(기본값)이 남아 있으면
			// 다음 Tick 에서 EditorWorldController 가 카메라를 되돌려 버리므로
			// 새 위치로 동기화한다.
			EditorEngine->GetViewportLayout().GetViewportClient(0).SyncCameraTarget();
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

    if (!EditorEngine) return;

    UWorld* World = EditorEngine->GetFocusedWorld();
    if (!World) return; // Early Exit: 전체 코드의 들여쓰기 깊이를 한 단계 줄임

    const TArray<AActor*>& Actors = World->GetActors();

    // LastClickedActorIndex 유효성 검사
    if (LastClickedActorIndex >= static_cast<int32>(Actors.size()))
    {
        LastClickedActorIndex = -1;
    }

    ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);
    ImGui::Begin("Scene Manager");

    ImGui::Text("Actors (%d)", static_cast<int32>(Actors.size()));
    ImGui::Separator();

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();

    // ctrl 클릭, ctrl + shift 클릭, shift 클릭, 기본 클릭 4가지 상태에 따라 각각 처리하는 람다 함수입니다.
    auto HandleActorSelection = [&](AActor* SelectedActor, int32 CurrentIndex)
    {
        const bool bShiftDown = ImGui::GetIO().KeyShift;
        const bool bCtrlDown  = ImGui::GetIO().KeyCtrl;

        // 기준점이 유효한지 확인 (이전 클릭 내역)
        const bool bValidRange = (LastClickedActorIndex >= 0) &&
                                 (LastClickedActorIndex < static_cast<int32>(Actors.size()));

        if (bCtrlDown && bShiftDown)
        {
            // 1. Ctrl + Shift + Click : 기존 선택을 유지한 채로 범위 안의 액터들을 '추가'
            if (bValidRange)
            {
                const int32 Start = std::min(LastClickedActorIndex, CurrentIndex);
                const int32 End   = std::max(LastClickedActorIndex, CurrentIndex);

                for (int32 i = Start; i <= End; ++i)
                {
                    if (AActor* RangeActor = Actors[i])
                    {
                        // 선택되어 있지 않은 경우에만 Toggle하여 '추가'되도록 보장
                        if (!Selection.IsSelected(RangeActor))
                        {
                            Selection.ToggleSelect(RangeActor);
                        }
                    }
                }
            }
            else
            {
                // 기준점이 없으면 단일 추가 선택으로 Fallback
                if (!Selection.IsSelected(SelectedActor)) Selection.ToggleSelect(SelectedActor);
                LastClickedActorIndex = CurrentIndex;
            }
        }
        else if (bShiftDown)
        {
            // 2. Shift + Click : 기존 선택을 모두 해제하고 새로운 범위만 선택
            if (bValidRange)
            {
                Selection.ClearSelection();

                const int32 Start = std::min(LastClickedActorIndex, CurrentIndex);
                const int32 End   = std::max(LastClickedActorIndex, CurrentIndex);

                for (int32 i = Start; i <= End; ++i)
                {
                    if (AActor* RangeActor = Actors[i])
                    {
                        Selection.ToggleSelect(RangeActor); 
                    }
                }
            }
            else
            {
                // 기준점이 없으면 단일 선택으로 Fallback
                Selection.Select(SelectedActor);
                LastClickedActorIndex = CurrentIndex;
            }
        }
        else if (bCtrlDown)
        {
            // 3. Ctrl + Click : 개별 액터 추가/해제 (토글)
            Selection.ToggleSelect(SelectedActor);
            LastClickedActorIndex = CurrentIndex; // 범위 선택의 새로운 기준점이 됨
        }
        else
        {
            // 4. Click : 단일 선택
            Selection.Select(SelectedActor);
            LastClickedActorIndex = CurrentIndex; // 범위 선택의 새로운 기준점이 됨
        }
    };

    // UI 렌더링 영역
    ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

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
            
            // Selectable이 클릭되었을 때만 로직 호출
            if (ImGui::Selectable(ActorName.c_str(), bIsSelected))
            {
                HandleActorSelection(Actor, i);
            }

            ImGui::PopID();
        }
    }

    Clipper.End();
    ImGui::EndChild();
    ImGui::End(); // Begin("Scene Manager")에 대한 End
}