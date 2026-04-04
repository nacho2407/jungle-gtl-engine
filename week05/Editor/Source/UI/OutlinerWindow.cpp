#include "OutlinerWindow.h"

#include "imgui.h"
#include "EditorEngine.h"
#include "Core/ShowFlags.h"
#include "Core/ViewportClient.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"

void FOutlinerWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	const bool bOpen = ImGui::Begin("Outliner");
	ImGui::PopStyleVar();
	if (!bOpen)
	{
		ImGui::End();
		return;
	}
	if (!Engine || !Engine->GetScene())
	{
		ImGui::End();
		return;
	}


	AActor* SelectedActor = Engine->GetSelectedActor();

	ImGui::SeparatorText("Actors");

	UScene* Scene = Engine->GetScene();
	const TArray<AActor*>& Actors = Scene->GetActors();
	
	// GameJam
//	for (AActor* Actor : Actors)
//	{
//;
//		if (!Actor || Actor->IsPendingDestroy())
//		{
//			continue;
//		}
//
//		const bool bSelected = (Actor == SelectedActor);
//		ImGui::PushID(Actor);
//		bool bVisible = Actor->IsVisible();
//		if (ImGui::Checkbox("##visible", &bVisible))
//		{
//			Actor->SetVisible(bVisible);
//		}
//		ImGui::SameLine();
//
//		if (ImGui::Selectable(Actor->GetName().c_str(), bSelected))
//		{
//			Engine->SetSelectedActor(Actor);
//		}
//		ImGui::PopID();
//	}

	ImGuiListClipper Clipper;
	Clipper.Begin((int)Actors.size());

	while (Clipper.Step())
	{
		for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
		{
			AActor* Actor = Actors[i];
			if (!Actor || Actor->IsPendingDestroy()) continue;

			const bool bSelected = (Actor == SelectedActor);
			ImGui::PushID(Actor);

			if (ImGui::Selectable("##sel", bSelected))
			{
				Engine->SetSelectedActor(Actor);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(Actor->GetName().c_str());

			ImGui::PopID();
		}
	}
	Clipper.End();


	ImGui::End();

}
