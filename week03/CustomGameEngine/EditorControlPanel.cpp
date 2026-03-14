#include "EditorControlPanel.h"
#include "ImGui/imgui.h"
#include "FVector.h"
#include "TimerManager.h"
#include "SceneManager.h"
#include "World.h"
#include "UCubeComp.h"
#include "UCamera.h"
#include <string>

// Remove later.
enum class PrimitiveType
{
	Triangle,
	Cube,
	Sphere
};

// Remove later.
static PrimitiveType type = PrimitiveType::Triangle;
static int spawnCount = 0;
static char sceneName[128] = "Default";
static bool orthogonal = false;
static float fov = 45.0f;
static FVector cameraLocation = { 0.0f, 0.0f, 0.0f };
static FVector cameraRotation = { 0.0f, 0.0f, 0.0f };

EditorControlPanel::EditorControlPanel()
{
}

EditorControlPanel::~EditorControlPanel()
{
}

void EditorControlPanel::Draw(UCamera* camera)
{
	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Control Panel");
	ImGui::Text("Hello Jungle World!");
	ImGui::Text("FPS %d (%d ms)",
		TimerManager::GetInstance().GetGlobalFPS(),
		static_cast<int>(TimerManager::GetInstance().GetGlobalDeltaTime() * 1000));
	ImGui::Separator();

	if (ImGui::Combo("Primitive", (int*)&type, "Triangle\0Cube\0Sphere\0"))
	{
		// TODO: Change primitive type to spawn.
	}
	if (ImGui::Button("Spawn"))
	{
		FVector pos = { 0, 0, 0 }, delta = { 1, 0, 0 };
		for (int i = 0; i < spawnCount; i++)
		{
			// TODO: Apply primitive type to spawn
			auto p = GetWorld().AddSceneComponent<UCubeComp>();
			p->RelativeLocation = pos;
			pos = pos + delta;
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("Number of spawn", &spawnCount);
	ImGui::SameLine();
	ImGui::Separator();

	ImGui::InputText("Scene Name", sceneName, 128);
	if (ImGui::Button("New Scene"))
	{
		SceneManager::GetInstance().NewScene();
	}
	if (ImGui::Button("Save Scene"))
	{
		SceneManager::GetInstance().SaveScene(sceneName);
	}
	if (ImGui::Button("Load Scene"))
	{
		SceneManager::GetInstance().LoadScene(sceneName);
	}
	ImGui::Separator();

	ImGui::Checkbox("Orthogonal", &camera->IsOrthogonal); // TODO: Connect orthogonal property.
	ImGui::InputFloat("FOV", &camera->FOV);
	ImGui::InputFloat3("Camera Location", &camera->Position.x);
	ImGui::InputFloat3("Camera Rotation", &camera->Pitch);

	ImGui::End();
}
