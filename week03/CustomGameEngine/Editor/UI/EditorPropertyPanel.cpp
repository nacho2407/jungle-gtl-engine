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

	if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && !editor->GetSelectedComponents().empty())
	{
		editor->RemoveSelectedComponents();
	}

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
			editor->RemoveSelectedComponents();
		}
	}

	ImGui::End();
}

void EditorPropertyPanel::RemoveSelected()
{
	editor->RemoveSelectedComponents();
}
