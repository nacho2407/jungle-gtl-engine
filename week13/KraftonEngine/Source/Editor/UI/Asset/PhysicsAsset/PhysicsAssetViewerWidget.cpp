#include "PhysicsAssetViewerWidget.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include <imgui.h>

bool FPhysicsAssetViewerWidget::CanEdit(UObject* Object) const
{
	return dynamic_cast<UPhysicsAsset*>(Object) != nullptr;
}

void FPhysicsAssetViewerWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	SelectedBodyIndex = -1;
}

void FPhysicsAssetViewerWidget::Close()
{
	FAssetEditorWidget::Close();
	SelectedBodyIndex = -1;
}

void FPhysicsAssetViewerWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
	if (!PhysicsAsset)
	{
		return;
	}

	bool bWindowOpen = true;
	FString Title = "Physics Asset Viewer";
	if (IsDirty())
	{
		Title += " *";
	}

	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(Title.c_str(), &bWindowOpen))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	ImGui::Columns(2);
	RenderBodyList(PhysicsAsset);
	ImGui::NextColumn();
	RenderBodyDetails(PhysicsAsset);
	ImGui::Columns(1);

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FPhysicsAssetViewerWidget::RenderBodyList(UPhysicsAsset* PhysicsAsset)
{
	ImGui::TextUnformatted("Bodies");
	ImGui::Separator();

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	for (int32 Index = 0; Index < static_cast<int32>(Bodies.size()); ++Index)
	{
		const UBodySetup* Body = Bodies[Index];
		if (!Body)
		{
			continue;
		}

		const FKAggregateGeom& AggGeom = Body->GetAggGeom();

		FString Label = "[" + std::to_string(Index) + "] ";
		Label += Body->BoneName.ToString();
		Label += "  S:";
		Label += std::to_string(AggGeom.SphereElems.size());
		Label += " B:";
		Label += std::to_string(AggGeom.BoxElems.size());
		Label += " C:";
		Label += std::to_string(AggGeom.SphylElems.size());
		Label += " X:";
		Label += std::to_string(AggGeom.ConvexElems.size());

		if (ImGui::Selectable(Label.c_str(), SelectedBodyIndex == Index))
		{
			SelectedBodyIndex = Index;
		}
	}
}

void FPhysicsAssetViewerWidget::RenderBodyDetails(UPhysicsAsset* PhysicsAsset)
{
	ImGui::TextUnformatted("Body Details");
	ImGui::Separator();

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (SelectedBodyIndex < 0 || SelectedBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		ImGui::TextDisabled("Select a body.");
		return;
	}

	const UBodySetup* Body = Bodies[SelectedBodyIndex];
	if (!Body)
	{
		ImGui::TextDisabled("Invalid body.");
		return;
	}

	const FKAggregateGeom& AggGeom = Body->GetAggGeom();

	ImGui::Text("BoneName: %s", Body->BoneName.ToString().c_str());
	ImGui::Text("SphereElems: %zu", AggGeom.SphereElems.size());
	ImGui::Text("BoxElems: %zu", AggGeom.BoxElems.size());
	ImGui::Text("SphylElems: %zu", AggGeom.SphylElems.size());
	ImGui::Text("ConvexElems: %zu", AggGeom.ConvexElems.size());
}
