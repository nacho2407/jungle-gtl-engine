#include "EditorControlPanel.h"
#include "Editor.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "PrimitiveFactory.h"
#include "TimerManager.h"
#include "World.h"
#include "Math/Vector.h"
#include "ImGui/imgui.h"
#include <string>

static EPrimitiveType primitiveType = EPrimitiveType::Triangle;
static int spawnCount = 0;
static char sceneName[128] = "Default";
static bool orthogonal = false;
static float fov = 45.0f;
static FVector cameraLocation = { 0.0f, 0.0f, 0.0f };
static FVector cameraRotation = { 0.0f, 0.0f, 0.0f };

EditorControlPanel::EditorControlPanel(Editor* editorDependency) : editor(editorDependency)
{
}

EditorControlPanel::~EditorControlPanel()
{
}

void EditorControlPanel::Draw(UCameraComponent* camera)
{
	ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Control Panel");
	ImGui::Text("Hello Jungle World!");
	ImGui::Text("FPS %d (%d ms)",
		TimerManager::GetInstance().GetGlobalFPS(),
		static_cast<int>(TimerManager::GetInstance().GetGlobalDeltaTime() * 1000));
	ImGui::Separator();

	if (ImGui::Combo("Primitive", (int*)&primitiveType, "Sphere\0Cube\0Triangle\0Plane\0"))
	{
		// TODO: Change primitive type to spawn.
	}
	if (ImGui::Button("Spawn"))
	{
		for (int i = 0; i < spawnCount; i++)
		{
			// TODO: Apply primitive type to spawn
			auto p = PrimitiveFactory::AddPrimitive(primitiveType);
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("Number of spawn", &spawnCount);
	ImGui::SameLine();
	ImGui::Separator();

	ImGui::InputText("Scene Name", sceneName, 128);
	if (ImGui::Button("New Scene"))
	{
		editor->SelectComponent(nullptr);
		GetWorld().NewScene();
	}
	if (ImGui::Button("Save Scene"))
	{
		GetWorld().SaveScene(sceneName);
	}
	if (ImGui::Button("Load Scene"))
	{
		editor->SelectComponent(nullptr);
		GetWorld().LoadScene(sceneName);
	}
	ImGui::Separator();

	ImGui::Checkbox("Orthogonal", &camera->IsOrthogonal);
	ImGui::InputFloat("FOV", &camera->FOV);
	ImGui::InputFloat3("Camera Location", &camera->Position.x);
	ImGui::InputFloat3("Camera Rotation", &camera->Pitch);
	ImGui::InputFloat3("Camera Direction", &camera->Direction.x);
	ImGui::InputFloat("Camera Speed", &camera->MoveSpeed);

	ImGui::End();
}
