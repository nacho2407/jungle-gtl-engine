#include "PhysicsAssetViewerWidget.h"

#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsAssetManager.h"
#include "Editor/UI/Util/InlinePropertyRenderer.h"
#include "Core/Logging/Log.h"

#include <imgui.h>

namespace
{
FString BuildPhysicsBodyTreeLabel(const FString& BoneName, int32 BodyIndex, const UBodySetup* BodySetup)
{
	FString Label = BoneName;
	const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	Label += "  [Body ";
	Label += std::to_string(BodyIndex);
	Label += "] S:";
	Label += std::to_string(AggGeom.SphereElems.size());
	Label += " B:";
	Label += std::to_string(AggGeom.BoxElems.size());
	Label += " C:";
	Label += std::to_string(AggGeom.SphylElems.size());
	Label += " X:";
	Label += std::to_string(AggGeom.ConvexElems.size());
	return Label;
}

bool RenderConstraintInitDescDetails(UPhysicsAsset* PhysicsAsset, const UBodySetup* BodySetup)
{
	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::TextUnformatted("Constraint");

	FConstraintInstanceInitDesc* ConstraintDesc =
		(PhysicsAsset && BodySetup)
			? PhysicsAsset->FindConstraintInitDescByChildBoneName(BodySetup->BoneName)
			: nullptr;
	if (!ConstraintDesc)
	{
		ImGui::TextDisabled("None");
		return false;
	}

	return FInlinePropertyRenderer::RenderStructProperties(
		FConstraintInstanceInitDesc::StaticStruct(),
		ConstraintDesc,
		PhysicsAsset,
		"##PhysicsAssetViewerConstraintProps");
}

bool HasPhysicsBodyInSubtree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex)
{
	if (!Asset || !PhysicsAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
	{
		return false;
	}

	const FBone& Bone = Asset->Bones[BoneIndex];
	if (PhysicsAsset->FindBodyIndexByBoneName(FName(Bone.Name)) != -1)
	{
		return true;
	}

	for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == BoneIndex &&
			HasPhysicsBodyInSubtree(Asset, PhysicsAsset, Index))
		{
			return true;
		}
	}

	return false;
}
}

bool FPhysicsAssetViewerWidget::CanEdit(UObject* Object) const
{
	return dynamic_cast<UPhysicsAsset*>(Object) != nullptr;
}

void FPhysicsAssetViewerWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	const UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(Object);
	SourceSkeletalMesh = PhysicsAsset ? PhysicsAsset->GetTypedOuter<USkeletalMesh>() : nullptr;
	SelectedBodyIndex = -1;
}

void FPhysicsAssetViewerWidget::Close()
{
	FAssetEditorWidget::Close();
	SourceSkeletalMesh = nullptr;
	SelectedBodyIndex = -1;
}

void FPhysicsAssetViewerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAssetEditorWidget::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(SourceSkeletalMesh);
}

bool FPhysicsAssetViewerWidget::SavePhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	if (SourceSkeletalMesh)
	{
		return FPhysicsAssetManager::Get().SaveForSkeletalMesh(
			SourceSkeletalMesh,
			SourceSkeletalMesh->GetAssetPathFileName());
	}

	const FString& PhysicsAssetPath = PhysicsAsset->GetAssetPathFileName();
	if (PhysicsAssetPath.empty() || PhysicsAssetPath == "None")
	{
		return false;
	}

	return FPhysicsAssetManager::Get().Save(
		PhysicsAsset,
		PhysicsAssetPath,
		PhysicsAsset->GetSourceSkeletalMeshPath());
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
	if (SelectedBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		SelectedBodyIndex = -1;
	}

	const FSkeletalMesh* Asset = SourceSkeletalMesh ? SourceSkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		ImGui::TextDisabled("No source skeletal mesh.");
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == -1)
		{
			RenderBodyTree(Asset, PhysicsAsset, Index);
		}
	}
}

bool FPhysicsAssetViewerWidget::RenderBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex)
{
	if (!Asset || !PhysicsAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
	{
		return false;
	}

	const FBone& Bone = Asset->Bones[BoneIndex];
	const int32 BodyIndex = PhysicsAsset->FindBodyIndexByBoneName(FName(Bone.Name));
	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	const UBodySetup* Body = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size()))
		? Bodies[BodyIndex]
		: nullptr;

	if (!Body)
	{
		bool bRenderedAny = false;
		for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
		{
			if (Asset->Bones[Index].ParentIndex == BoneIndex &&
				RenderBodyTree(Asset, PhysicsAsset, Index))
			{
				bRenderedAny = true;
			}
		}
		return bRenderedAny;
	}

	bool bHasVisibleChildren = false;
	for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == BoneIndex &&
			HasPhysicsBodyInSubtree(Asset, PhysicsAsset, Index))
		{
			bHasVisibleChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (Body && BodyIndex == SelectedBodyIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!bHasVisibleChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	FString Label = BuildPhysicsBodyTreeLabel(Bone.Name, BodyIndex, Body);
	Label += "##PhysicsAssetViewerBodyBone";
	Label += std::to_string(BoneIndex);

	const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SelectedBodyIndex = BodyIndex;
	}

	if (bOpen && bHasVisibleChildren)
	{
		for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
		{
			if (Asset->Bones[Index].ParentIndex == BoneIndex)
			{
				RenderBodyTree(Asset, PhysicsAsset, Index);
			}
		}
		ImGui::TreePop();
	}

	return true;
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
	if (RenderConstraintInitDescDetails(PhysicsAsset, Body))
	{
		if (SavePhysicsAsset(PhysicsAsset))
		{
			ClearDirty();
		}
		else
		{
			UE_LOG("PhysicsAsset constraint edit warning: failed to persist constraint. PhysicsAsset=%s", PhysicsAsset->GetAssetPathFileName().c_str());
			MarkDirty();
		}
	}
}
