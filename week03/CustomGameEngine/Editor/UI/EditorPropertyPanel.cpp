#include "EditorPropertyPanel.h"
EditorPropertyPanel::EditorPropertyPanel(Editor* parent) : editor(parent)
{
}

EditorPropertyPanel::~EditorPropertyPanel()
{
}

void EditorPropertyPanel::Draw(USceneComponent* selectedComponent)
{
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
	editor->SelectSingleComponent(nullptr);
	editor->SelectMultiComponent(nullptr);
	GetWorld().GetSceneManager()->RemoveSceneComponent(selectedComponent);
}
