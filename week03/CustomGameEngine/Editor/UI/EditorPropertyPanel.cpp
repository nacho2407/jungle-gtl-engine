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
			RemoveSelected();
		}
	}

	ImGui::End();
}

void EditorPropertyPanel::RemoveSelected()
{
	TArray<USceneComponent*> selectedComponents = editor->GetSelectedComponents();
	editor->ClearSelection();

	SceneManager* sceneManager = GetWorld().GetSceneManager();
	for (USceneComponent* comp : selectedComponents)
	{
		if (comp == nullptr || comp->Scene == nullptr)
		{
			continue;
		}
		sceneManager->RemoveSceneComponent(comp, comp->Scene);
	}
}
