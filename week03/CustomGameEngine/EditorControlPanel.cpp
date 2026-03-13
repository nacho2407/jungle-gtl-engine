#include "EditorControlPanel.h"
#include "ImGui/imgui.h"
#include "FVector.h"
#include "TimerManager.h"
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

void EditorControlPanel::Draw()
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
		// TODO: Spawn primitive #spawnCount times in scene.
	}
	ImGui::SameLine();
	ImGui::InputInt("Number of spawn", &spawnCount);
	ImGui::SameLine();
	ImGui::Separator();

	ImGui::InputText("Scene Name", sceneName, 128);
	if (ImGui::Button("New Scene"))
	{

	}
	if (ImGui::Button("Save Scene"))
	{

	}
	if (ImGui::Button("Load Scene"))
	{

	}
	ImGui::Separator();

	ImGui::Checkbox("Orthogonal", &orthogonal);
	ImGui::InputFloat("FOV", &fov);
	ImGui::InputFloat3("Camera Location", &cameraLocation.x);
	ImGui::InputFloat3("Camera Rotation", &cameraRotation.x);

	ImGui::End();
}
