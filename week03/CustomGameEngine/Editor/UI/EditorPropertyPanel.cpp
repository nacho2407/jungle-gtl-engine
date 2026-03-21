#include "EditorPropertyPanel.h"
EditorPropertyPanel::EditorPropertyPanel(Editor* parent) : editor(parent)
{
}

EditorPropertyPanel::~EditorPropertyPanel()
{
}

void EditorPropertyPanel::Draw(USceneComponent* selectedComponent, ImGuiWindowLayout& layout)
{
	ImGui::SetNextWindowPos(layout.PosXY, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(layout.SizeWH, ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Property Panel");

	if (selectedComponent)
	{
		selectedComponent->DrawProperties();
		
		if (selectedComponent->IsA(UPrimitiveComponent::GetClass()))
		{
			UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(selectedComponent);
			primitive->MarkRenderStateDirty();
		}

		if (ImGui::Button("Delete", { 100, 30 }))
		{
			RemoveSelected(selectedComponent);
		}
	}

	ImGui::End();
}

void EditorPropertyPanel::RemoveSelected(USceneComponent* selectedComponent)
{
	GetWorld().GetSceneManager()->RemoveSceneComponent(selectedComponent);
	editor->SelectComponent(nullptr);
}
