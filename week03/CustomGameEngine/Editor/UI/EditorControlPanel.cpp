#include "EditorControlPanel.h"

static EPrimitiveType primitiveType = EPrimitiveType::Triangle;
static int spawnCount = 0;
static char sceneName[128] = "Default";
static bool orthogonal = false;
static float fov = 45.0f;
static FVector cameraLocation = { 0.0f, 0.0f, 0.0f };
static FVector cameraRotation = { 0.0f, 0.0f, 0.0f };

static float frameCount = 0;
static float elapsedTime = 0.0f;
static float displayFPS = 0.0f;
const float fpsUpdateInterval = 0.5f; // 초 단위

EditorControlPanel::EditorControlPanel(Editor* parent, URenderer* InRenderer) : editor(parent), Renderer(InRenderer)
{
}

EditorControlPanel::~EditorControlPanel()
{
}

namespace
{
	static TArray<FString> GetSceneFileList(const FString& sceneDir)
	{
		TArray<FString> SceneList;

		namespace fs = std::filesystem;

		if (!fs::exists(sceneDir) || !fs::is_directory(sceneDir))
		{
			return SceneList;
		}

		for (const auto& Entry : fs::directory_iterator(sceneDir))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const fs::path& Path = Entry.path();

			const FString extension = Path.extension().string();
			if (extension == ".Scene")
			{
				SceneList.push_back(Path.stem().string());
			}
		}

		std::sort(SceneList.begin(), SceneList.end());
		return SceneList;
	}
} // namespace

void UpdateFPS(float deltaTime)
{
	frameCount++;
	elapsedTime += deltaTime;

	if (elapsedTime >= fpsUpdateInterval)
	{
		displayFPS = frameCount / elapsedTime;

		frameCount = 0;
		elapsedTime = 0.0f;
	}
}

void EditorControlPanel::Draw(UCameraComponent* camera, UGridComp* Grid)
{
	UpdateFPS(TimerManager::GetInstance().GetGlobalDeltaTime());

	ImGui::Begin("Jungle Control Panel");
	ImGui::Text("Hello Jungle World!");
	ImGui::Text("FPS %.0f (%d ms)",
		displayFPS,
		static_cast<int>(TimerManager::GetInstance().GetGlobalDeltaTime() * 1000));
	ImGui::Separator();

	if (ImGui::Combo("Primitive", (int*)&primitiveType, "Sphere\0Cube\0Triangle\0Plane\0"))
	{
		// TODO: Change primitive type to spawn.
	}
	if (ImGui::Button("Spawn"))
	{
		for (int i = 0; i < spawnCount; ++i)
		{
			// TODO: Apply primitive type to spawn
			auto p = PrimitiveFactory::AddPrimitive(primitiveType);
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("Number of spawn", &spawnCount);
	ImGui::SameLine();
	ImGui::Separator();

    static TArray<FString> SceneList;
    static int32 SelectedSceneIndex = -1;
    static bool bCreateNewSceneAfterSave = false;
    SceneManager* sceneMgr = GetWorld().GetSceneManager();

    if (sceneMgr == nullptr)
    {
		ImGui::Text("Scene manager is not initialized.");
		ImGui::End();
		return;
    }

    if (ImGui::Button("New Scene"))
    {
		sceneMgr->NewScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Scene"))
    {
        ImGui::OpenPopup("Save Scene Popup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene"))
    {
        SceneList = GetSceneFileList("Scenes/");
        SelectedSceneIndex = -1;
        ImGui::OpenPopup("Load Scene Popup");
    }
    ImGui::SetNextWindowSize(ImVec2(360, 140), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Save Scene Popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Scene Name", sceneName, IM_ARRAYSIZE(sceneName));

        if (ImGui::Button("Save"))
        {
            editor->SelectSingleComponent(nullptr);
            editor->SelectMultiComponent(nullptr);
            sceneMgr->SaveScene(sceneName);

            ImGui::CloseCurrentPopup();

            if (bCreateNewSceneAfterSave)
            {
                bCreateNewSceneAfterSave = false;
                sceneMgr->NewScene();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            bCreateNewSceneAfterSave = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Load Scene 팝업
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Load Scene Popup", nullptr))
    {
        if (ImGui::BeginChild("SceneList", ImVec2(0, 220), true))
        {
            for (int32 i = 0; i < (int32)SceneList.size(); ++i)
            {
                const bool bSelected = (SelectedSceneIndex == i);
                if (ImGui::Selectable(SceneList[i].c_str(), bSelected))
                {
                    SelectedSceneIndex = i;
                }
            }
            ImGui::EndChild();
        }

        const bool bCanLoad = (SelectedSceneIndex >= 0 && SelectedSceneIndex < (int32)SceneList.size());

        if (!bCanLoad)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Load"))
        {
            editor->SelectSingleComponent(nullptr);
            editor->SelectMultiComponent(nullptr);
            sceneMgr->LoadScene(SceneList[SelectedSceneIndex].c_str());
            ImGui::CloseCurrentPopup();
        }

        if (!bCanLoad)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

	ImGui::Separator();

	if (camera)
	{
		ImGui::Checkbox("Orthogonal", &camera->IsOrthogonal);
		ImGui::InputFloat("FOV", &camera->FOV);

		FVector camPosition = camera->GetRelativeLocation();
		FRotator camRotation = camera->GetRelativeRotation();
		ImGui::InputFloat3("Camera Location", &camPosition.x);
		ImGui::InputFloat3("Camera Rotation", &camRotation.Roll);
		camera->SetRelativeLocation(camPosition);
		camera->SetRelativeRotation(camRotation);

		ImGui::InputFloat3("Camera Direction", &camera->Direction.x);
		ImGui::SliderFloat("Camera Move Speed", &camera->MoveSpeed, 0.0f, 20.0f, "%.0f");
		ImGui::SliderFloat("Camera Rotation Speed", &camera->RotateSensitivity, 0.0f, 20.0f, "%.0f");
	}

	ImGui::Separator();
	ImGui::Text("Grid Settings");

	bool bChanged = false;
	if (ImGui::DragFloat("Half Extent", &Grid->HalfExtent, 1.0f, 1.0f, 1000.0f)) bChanged = true;
	if (ImGui::DragFloat("Cell Size", &Grid->CellSize, 0.1f, 1.0f, 10.0f)) bChanged = true;
	if (bChanged)
	{
		ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
	}

	ImGui::Separator();
	if (Renderer)
	{
		ImGui::Checkbox("Show Primitives", &Renderer->ShowFlagMap[EEngineShowFlags::SF_Primitives]);
		ImGui::Checkbox("Show Billboard Text", &Renderer->ShowFlagMap[EEngineShowFlags::SF_BillboardText]);
		ImGui::Checkbox("Show Editor Primitives", &Renderer->ShowFlagMap[EEngineShowFlags::SF_EditorPrimitives]);
	}
	static int32 RenderingMode = 0;
	if(ImGui::Combo("RenderingMode", &RenderingMode, "Normal\0NoCull\0Wireframe\0"))
	{
		Renderer->SetRasterizerMode(RenderingMode);
	}
	//for (auto& elem : Renderer->ShowFlagMap)
	//{
	//	ImGui::Checkbox(elem.first.c_str(), &elem.second);
	//}

	ImGui::End();
}
