#include "Editor/UI/Panel/EditorReflectionPropertyWidget.h"

#include "Editor/Selection/SelectionManager.h"

#include "ImGui/imgui.h"

void FEditorReflectionPropertyWidget::Render(const FEditorPanelContext& Context)
{
	if (!ImGui::Begin("Reflection Property Window"))
	{
		ImGui::End();
		return;
	}

	if (!Context.SelectionManager)
	{
		ImGui::TextDisabled("No object selected.");
		ImGui::End();
		return;
	}

	Renderer.Render(Context.SelectionManager->GetSelectedDetailTargets());
	ImGui::End();
}
