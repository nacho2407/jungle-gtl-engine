#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Core/Common.h"
#include "GameFramework/WorldContext.h"
#include "Component/CameraComponent.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Serialization/SceneSaveManager.h"
#include "Profiling/Stats.h"

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
		// PIE 중이면 먼저 종료해 에디터 월드를 활성화한 뒤 저장.
		EditorEngine->StopPlayInEditorImmediate();
		FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
		if (Ctx)
		{
			UCameraComponent* PerspectiveCam = nullptr;
			for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
			{
				if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective || VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
				{
					PerspectiveCam = VC->GetCamera();
					break;
				}
			}
			FSceneSaveManager::SaveSceneAsJSON(SceneName, *Ctx, PerspectiveCam);
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
			FPerspectiveCameraData CamData;
			FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, CamData);
			if (LoadCtx.World)
			{
				EditorEngine->GetWorldList().push_back(LoadCtx);
				EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
				EditorEngine->GetSelectionManager().SetWorld(LoadCtx.World);
				LoadCtx.World->WarmupPickingData(); // 씬 로드 후 메시 BVH와 월드 primitive BVH를 모두 빌드
			}
			EditorEngine->ResetViewport();

			// ResetViewport()가 카메라를 기본값으로 초기화하므로 그 이후에 복원
			if (CamData.bValid)
			{
				for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
				{
					if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective || VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
					{
						if (UCameraComponent* Cam = VC->GetCamera())
						{
							Cam->SetWorldLocation(CamData.Location);
							Cam->SetRelativeRotation(CamData.Rotation);
							FCameraState CS = Cam->GetCameraState();
							CS.FOV = CamData.FOV;
							CS.NearZ = CamData.NearClip;
							CS.FarZ = CamData.FarClip;
							Cam->SetCameraState(CS);
						}
						break;
					}
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
	RenderActorOutliner();

	ImGui::End();
}

void FEditorSceneWidget::RenderActorOutliner()
{
	SCOPE_STAT_CAT("SceneWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World) return;

	const TArray<AActor*>& Actors = World->GetActors();

	// null이 아닌 유효 Actor 인덱스만 수집 (Clipper는 연속 인덱스 필요)
	ValidActorIndices.clear();
	ValidActorIndices.reserve(Actors.size());
	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		if (Actors[i]) ValidActorIndices.push_back(i);
	}

	ImGui::Text("Actors (%d)", static_cast<int32>(ValidActorIndices.size()));
	ImGui::Separator();

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();

	ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

	ImGuiListClipper Clipper;
	Clipper.Begin(static_cast<int>(ValidActorIndices.size()));
	while (Clipper.Step())
	{
		for (int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row)
		{
			AActor* Actor = Actors[ValidActorIndices[Row]];

			const FString& StoredName = Actor->GetFName().ToString();
			const char* DisplayName = StoredName.empty()
				? Actor->GetTypeInfo()->name
				: StoredName.c_str();

			bool bIsSelected = Selection.IsSelected(Actor);
			if (ImGui::Selectable(DisplayName, bIsSelected))
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
	}

	ImGui::EndChild();
}
