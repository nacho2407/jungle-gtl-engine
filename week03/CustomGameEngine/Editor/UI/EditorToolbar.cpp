#include "EditorToolbar.h"
#include "Component/GridComp.h"
#include "EngineCore/ResourceManager.h"
#include "ImGui/imgui.h"

EditorToolbar::EditorToolbar(Editor* parent) : editor(parent)
{
}

EditorToolbar::~EditorToolbar()
{
}

void EditorToolbar::Draw(UGridComp* Grid)
{
	UGizmoComponent* gizmoController = Gizmo::GetInstance().GetController();

	ImGui::Begin("Toolbar");

	USceneComponent* pickedObjecdts = editor->GetLastSelectedComponent();
	//픽킹된 오브젝트가 없으면 Toolbar에 아무것도 띄우지 않습니다
	if (!pickedObjecdts)
	{
		ImGui::End();
		return;
	}
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

	ImGui::End();
}
