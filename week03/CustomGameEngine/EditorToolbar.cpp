#include "EditorToolbar.h"
#include "ImGui/imgui.h"
#include "GizmoController.h"

EditorToolbar::EditorToolbar()
{
}

EditorToolbar::~EditorToolbar()
{
}

void EditorToolbar::Draw()
{
	ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(200, 30));
	if (ImGui::Button("Translate"))
	{
		Gizmo::GetInstance().GetController()->SetControlStrategy(GizmoControllerType::Translation);
	}
	ImGui::SameLine();
	if (ImGui::Button("Rotate"))
	{
		Gizmo::GetInstance().GetController()->SetControlStrategy(GizmoControllerType::Rotation);
	}
	ImGui::SameLine();
	if (ImGui::Button("Scale"))
	{
		Gizmo::GetInstance().GetController()->SetControlStrategy(GizmoControllerType::Scale);
	}
	ImGui::End();
}