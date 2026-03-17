#include "EditorToolbar.h"
#include "Gizmo.h"
#include "GizmoController.h"
#include "ImGui/imgui.h"

EditorToolbar::EditorToolbar()
{
}

EditorToolbar::~EditorToolbar()
{
}

void EditorToolbar::Draw()
{
	ImGui::Begin("Toolbar");

	bool bLocalSpace = Gizmo::GetInstance().GetController()->IsLocalSpace();
	if (bLocalSpace ? ImGui::Button("Local") : ImGui::Button("World"))
	{
		Gizmo::GetInstance().GetController()->SetLocalSpace(!bLocalSpace);
	}

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