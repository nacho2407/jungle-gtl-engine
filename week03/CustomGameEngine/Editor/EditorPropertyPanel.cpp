#include "EditorPropertyPanel.h"
#include "ImGui/imgui.h"
#include "Component/PrimitiveComponent.h"	
#include "Component/SceneComponent.h"

EditorPropertyPanel::EditorPropertyPanel()
{
}

EditorPropertyPanel::~EditorPropertyPanel()
{
}

void EditorPropertyPanel::Draw(USceneComponent* selectedComponent)
{
	ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Property Panel");

	if (selectedComponent)
	{
		selectedComponent->DrawProperties();
		
		if (selectedComponent->IsA(UPrimitiveComponent::GetClass()))
		{
			UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(selectedComponent);
			primitive->MarkRenderStateDirty();
		}
	}

	ImGui::End();
}
