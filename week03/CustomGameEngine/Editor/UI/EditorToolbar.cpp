#include "EditorToolbar.h"
#include "Component/GridComp.h"
#include "EngineCore/ResourceManager.h"

EditorToolbar::EditorToolbar()
{
}

EditorToolbar::~EditorToolbar()
{
}

void EditorToolbar::Draw(ImGuiWindowLayout& layout, UGridComp* Grid)
{
	ImGui::SetNextWindowPos(layout.PosXY, ImGuiCond_FirstUseEver);

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


	if (gizmoController->GetControlStrategyType() == GizmoControllerType::Rotation)
	{
		GizmoRotationControlStrategy* rotationStrategy = dynamic_cast<GizmoRotationControlStrategy*>(gizmoController->GetControlStrategy());
		bool bSnapping = rotationStrategy->bEnableSnap;
		ImGui::SameLine();
		if (!bSnapping ? ImGui::Button("Enable Snapping") : ImGui::Button("Disable Snapping"))
		{
			rotationStrategy->bEnableSnap = !bSnapping;
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

	if (Grid)
	{
		ImGui::Separator();
		ImGui::Text("Grid Settings");
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::DragFloat("Half Extent", &Grid->HalfExtent, 1.0f, 1.0f, 1000.0f))
		{
			ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::DragFloat("Cell Size", &Grid->CellSize, 0.1f, 0.1f, 10.0f))
		{
			ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
		}
	}

	ImGui::End();
}