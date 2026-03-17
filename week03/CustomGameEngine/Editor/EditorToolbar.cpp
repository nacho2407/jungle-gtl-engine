#include "EditorToolbar.h"
#include "Gizmo.h"
#include "Component/GizmoComponent.h"
#include "ImGui/imgui.h"

EditorToolbar::EditorToolbar()
{
}

EditorToolbar::~EditorToolbar()
{
}

void EditorToolbar::Draw()
{
	UGizmoComponent* gizmoController = Gizmo::GetInstance().GetController();

	ImGui::Begin("Toolbar");

	bool bLocalSpace = gizmoController->IsLocalSpace();
	bool bScaleMode = gizmoController->GetControlStrategyType() == GizmoControllerType::Scale;
	if (!bLocalSpace && !bScaleMode ? ImGui::Button("World") : ImGui::Button("Local"))
	{
		if (gizmoController->GetControlStrategyType() != GizmoControllerType::Scale)
		{
			gizmoController->SetLocalSpace(!bLocalSpace);
		}
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