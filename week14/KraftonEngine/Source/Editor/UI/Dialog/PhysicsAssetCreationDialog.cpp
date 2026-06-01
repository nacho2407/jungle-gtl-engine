#include "PhysicsAssetCreationDialog.h"

#include "ImGui/imgui.h"

namespace
{
	template<typename TEnum>
	void EnumCombo(const char* Label, TEnum& Value, const char* const* Items, int Count)
	{
		int Current = static_cast<int>(Value);
		if (ImGui::Combo(Label, &Current, Items, Count))
		{
			Value = static_cast<TEnum>(Current);
		}
	}
}

void FPhysicsAssetCreationDialog::Begin(FPhysicsAssetCreationDialogState& State, USkeletalMesh* SourceMesh, const FString& DirectoryPath, const FString& DefaultAssetName)
{
	State = FPhysicsAssetCreationDialogState{};
	State.SourceMesh = SourceMesh;
	State.TargetDirectoryPath = DirectoryPath;
	State.AssetName = DefaultAssetName;
	State.bOpenRequested = true;
}

EPhysicsAssetDialogResult FPhysicsAssetCreationDialog::Render(const char* PopupId, FPhysicsAssetCreationDialogState& State)
{
	if (State.bOpenRequested)
	{
		ImGui::OpenPopup(PopupId);
		State.bOpenRequested = false;
	}

	if (!ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return EPhysicsAssetDialogResult::None;
	}

	ImGui::PushID(PopupId);

	if (State.bCloseRequested)
	{
		State.bCloseRequested = false;
		State.Error.clear();
		ImGui::CloseCurrentPopup();
		ImGui::PopID();
		ImGui::EndPopup();
		return EPhysicsAssetDialogResult::None;
	}

	ImGui::TextUnformatted("New Physics Asset");
	ImGui::Separator();

	// 에셋 이름 (char 버퍼 왕복 — imgui_stdlib 의존 없이 처리)
	{
		char NameBuffer[256];
		std::snprintf(NameBuffer, sizeof(NameBuffer), "%s", State.AssetName.c_str());
		if (ImGui::InputText("Asset Name", NameBuffer, sizeof(NameBuffer)))
		{
			State.AssetName = NameBuffer;
		}
	}

	FPhysicsAssetCreationParams& P = State.Params;

	if (ImGui::CollapsingHeader("Body Creation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Min Bone Size", &P.MinBoneSize, 0.5f, 0.0f, 1000.0f);
		if (ImGui::TreeNodeEx("Advanced"))
		{
			ImGui::DragFloat("Min Weld Size", &P.MinWeldSize, 0.0001f, 0.0f, 1000.0f, "%.4f");
			ImGui::TreePop();
		}

		static const char* const PrimitiveItems[] = { "Box", "Capsule", "Sphere" };
		EnumCombo("Primitive Type", P.PrimitiveType, PrimitiveItems, 3);

		static const char* const WeightItems[] = { "Any Weight", "Dominant Weight" };
		EnumCombo("Vertex Weighting Type", P.VertexWeighting, WeightItems, 2);

		ImGui::Checkbox("Auto Orient to Bone", &P.bAutoOrientToBone);
		ImGui::Checkbox("Walk Past Small Bones", &P.bWalkPastSmallBones);
		ImGui::Checkbox("Create Body for All Bones", &P.bCreateBodyForAllBones);
		if (P.bCreateBodyForAllBones)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.32f, 1.0f), "All bones includes cloth/accessory chains.");
		}
		ImGui::InputInt("Lod Index", &P.LodIndex);
	}

	if (ImGui::CollapsingHeader("Constraint Creation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Create Constraints", &P.bCreateConstraints);

		static const char* const ConstraintItems[] = { "Free", "Limited", "Locked" };
		EnumCombo("Angular Constraint Mode", P.AngularConstraintMode, ConstraintItems, 3);
	}

	if (!State.Error.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", State.Error.c_str());
	}

	ImGui::Separator();
	EPhysicsAssetDialogResult Result = EPhysicsAssetDialogResult::None;

	if (ImGui::Button("Create Asset"))
	{
		State.Error.clear();
		if (!State.SourceMesh)
		{
			State.Error = "No source skeletal mesh.";
		}
		else if (State.AssetName.empty())
		{
			State.Error = "Asset name is empty.";
		}
		else
		{
			Result = EPhysicsAssetDialogResult::Submitted;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		State.Error.clear();
		ImGui::CloseCurrentPopup();
		Result = EPhysicsAssetDialogResult::Cancelled;
	}

	ImGui::PopID();
	ImGui::EndPopup();
	return Result;
}
