#include "Editor/UI/Asset/Physics/PhysicsAssetEditorWidget.h"

#include "Component/Debug/GizmoComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Asset/AssetRegistry.h"
#include "Editor/Subsystem/PhysicsAssetEditingLibrary.h"
#include "Editor/Subsystem/PhysicsAssetGenerator.h"
#include "Editor/Slate/SlateApplication.h"
#include "Editor/UI/Toolbar/ViewportToolbar.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetManager.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include "imgui_node_editor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace ed = ax::NodeEditor;

namespace
{
	uint32 GNextPhysicsAssetEditorInstanceId = 0;
	constexpr uint32 PhysicsGraphNodeBase = 1u;
	constexpr uint32 PhysicsGraphInputPinBase = 100000u;
	constexpr uint32 PhysicsGraphOutputPinBase = 200000u;
	constexpr uint32 PhysicsGraphLinkBase = 300000u;
	constexpr uint32 PhysicsGraphConstraintNodeBase = 400000u;
	constexpr uint32 PhysicsGraphConstraintInputPinBase = 500000u;
	constexpr uint32 PhysicsGraphConstraintOutputPinBase = 600000u;
	constexpr uint32 PhysicsGraphIdRange = 90000u;

	inline ed::NodeId ToPhysicsGraphNodeId(int32 BodyIndex)
	{
		return static_cast<ed::NodeId>(PhysicsGraphNodeBase + static_cast<uint32>(BodyIndex));
	}

	inline ed::PinId ToPhysicsGraphInputPinId(int32 BodyIndex)
	{
		return static_cast<ed::PinId>(PhysicsGraphInputPinBase + static_cast<uint32>(BodyIndex));
	}

	inline ed::PinId ToPhysicsGraphOutputPinId(int32 BodyIndex)
	{
		return static_cast<ed::PinId>(PhysicsGraphOutputPinBase + static_cast<uint32>(BodyIndex));
	}

	inline ed::LinkId ToPhysicsGraphLinkId(int32 ConstraintIndex)
	{
		return static_cast<ed::LinkId>(PhysicsGraphLinkBase + static_cast<uint32>(ConstraintIndex) * 2u);
	}

	inline ed::LinkId ToPhysicsGraphLinkedBodyLinkId(int32 ConstraintIndex)
	{
		return static_cast<ed::LinkId>(PhysicsGraphLinkBase + static_cast<uint32>(ConstraintIndex) * 2u + 1u);
	}

	inline ed::NodeId ToPhysicsGraphConstraintNodeId(int32 ConstraintIndex)
	{
		return static_cast<ed::NodeId>(PhysicsGraphConstraintNodeBase + static_cast<uint32>(ConstraintIndex));
	}

	inline ed::PinId ToPhysicsGraphConstraintInputPinId(int32 ConstraintIndex)
	{
		return static_cast<ed::PinId>(PhysicsGraphConstraintInputPinBase + static_cast<uint32>(ConstraintIndex));
	}

	inline ed::PinId ToPhysicsGraphConstraintOutputPinId(int32 ConstraintIndex)
	{
		return static_cast<ed::PinId>(PhysicsGraphConstraintOutputPinBase + static_cast<uint32>(ConstraintIndex));
	}

	inline uint32 PhysicsGraphNodeIdToU32(ed::NodeId Id)
	{
		return static_cast<uint32>(Id.Get());
	}

	inline uint32 PhysicsGraphPinIdToU32(ed::PinId Id)
	{
		return static_cast<uint32>(Id.Get());
	}

	inline uint32 PhysicsGraphLinkIdToU32(ed::LinkId Id)
	{
		return static_cast<uint32>(Id.Get());
	}

	int32 PhysicsGraphNodeIdToBodyIndex(ed::NodeId Id)
	{
		const uint32 Raw = PhysicsGraphNodeIdToU32(Id);
		return Raw >= PhysicsGraphNodeBase && Raw < PhysicsGraphNodeBase + PhysicsGraphIdRange
			? static_cast<int32>(Raw - PhysicsGraphNodeBase)
			: -1;
	}

	int32 PhysicsGraphNodeIdToConstraintIndex(ed::NodeId Id)
	{
		const uint32 Raw = PhysicsGraphNodeIdToU32(Id);
		return Raw >= PhysicsGraphConstraintNodeBase && Raw < PhysicsGraphConstraintNodeBase + PhysicsGraphIdRange
			? static_cast<int32>(Raw - PhysicsGraphConstraintNodeBase)
			: -1;
	}

	int32 PhysicsGraphPinIdToBodyIndex(ed::PinId Id, bool& bOutIsOutputPin)
	{
		const uint32 Raw = PhysicsGraphPinIdToU32(Id);
		if (Raw >= PhysicsGraphOutputPinBase && Raw < PhysicsGraphOutputPinBase + PhysicsGraphIdRange)
		{
			bOutIsOutputPin = true;
			return static_cast<int32>(Raw - PhysicsGraphOutputPinBase);
		}
		if (Raw >= PhysicsGraphInputPinBase && Raw < PhysicsGraphInputPinBase + PhysicsGraphIdRange)
		{
			bOutIsOutputPin = false;
			return static_cast<int32>(Raw - PhysicsGraphInputPinBase);
		}
		return -1;
	}

	int32 PhysicsGraphLinkIdToConstraintIndex(ed::LinkId Id)
	{
		const uint32 Raw = PhysicsGraphLinkIdToU32(Id);
		return Raw >= PhysicsGraphLinkBase && Raw < PhysicsGraphLinkBase + PhysicsGraphIdRange * 2u
			? static_cast<int32>((Raw - PhysicsGraphLinkBase) / 2u)
			: -1;
	}

	const char* ShapeTypeLabel(EPhysicsAssetShapeType ShapeType)
	{
		switch (ShapeType)
		{
		case EPhysicsAssetShapeType::Sphere: return "Sphere";
		case EPhysicsAssetShapeType::Box:    return "Box";
		case EPhysicsAssetShapeType::Sphyl:  return "Capsule";
		case EPhysicsAssetShapeType::Convex: return "Convex";
		default:                             return "Shape";
		}
	}

	const char* AngularModeLabel(EAngularConstraintMode Mode)
	{
		switch (Mode)
		{
		case EAngularConstraintMode::Free:    return "Free";
		case EAngularConstraintMode::Locked:  return "Locked";
		case EAngularConstraintMode::Limited:
		default:                              return "Limited";
		}
	}

	struct FPhysicsAssetPrimitiveSnapshot
	{
		FVector Center = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector BoxExtents = FVector(0.25f, 0.25f, 0.25f);
		float SphereRadius = 0.25f;
		float CapsuleRadius = 0.1f;
		float CapsuleLength = 0.3f;
	};

	float Max3(float A, float B, float C)
	{
		return (std::max)(A, (std::max)(B, C));
	}

	bool CapturePrimitiveSnapshot(
		const UBodySetup* Body,
		EPhysicsAssetShapeType ShapeType,
		int32 ShapeIndex,
		FPhysicsAssetPrimitiveSnapshot& OutSnapshot)
	{
		if (!Body || ShapeIndex < 0)
		{
			return false;
		}

		constexpr float MinShapeSize = 0.001f;
		const FKAggregateGeom& Geom = Body->GetAggGeom();
		switch (ShapeType)
		{
		case EPhysicsAssetShapeType::Sphere:
		{
			if (ShapeIndex >= static_cast<int32>(Geom.SphereElems.size())) return false;
			const FKSphereElem& Sphere = Geom.SphereElems[ShapeIndex];
			const float Radius = Clamp(Sphere.Radius, MinShapeSize, 1000.0f);
			OutSnapshot.Center = Sphere.Center;
			OutSnapshot.Rotation = FQuat::Identity;
			OutSnapshot.SphereRadius = Radius;
			OutSnapshot.BoxExtents = FVector(Radius, Radius, Radius);
			OutSnapshot.CapsuleRadius = Radius;
			OutSnapshot.CapsuleLength = (std::max)(Radius * 2.0f, MinShapeSize);
			return true;
		}
		case EPhysicsAssetShapeType::Box:
		{
			if (ShapeIndex >= static_cast<int32>(Geom.BoxElems.size())) return false;
			const FKBoxElem& Box = Geom.BoxElems[ShapeIndex];
			const FVector Extents(
				Clamp(Box.Extents.X, MinShapeSize, 1000.0f),
				Clamp(Box.Extents.Y, MinShapeSize, 1000.0f),
				Clamp(Box.Extents.Z, MinShapeSize, 1000.0f));
			const float Radius = Clamp((std::max)(Extents.X, Extents.Y), MinShapeSize, 1000.0f);
			const float TotalLength = Extents.Z * 2.0f;
			OutSnapshot.Center = Box.Center;
			OutSnapshot.Rotation = Box.Rotation.GetNormalized();
			OutSnapshot.SphereRadius = Clamp(Max3(Extents.X, Extents.Y, Extents.Z), MinShapeSize, 1000.0f);
			OutSnapshot.BoxExtents = Extents;
			OutSnapshot.CapsuleRadius = Radius;
			OutSnapshot.CapsuleLength = Clamp(TotalLength - Radius * 2.0f, MinShapeSize, 1000.0f);
			return true;
		}
		case EPhysicsAssetShapeType::Sphyl:
		{
			if (ShapeIndex >= static_cast<int32>(Geom.SphylElems.size())) return false;
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			const float Radius = Clamp(Capsule.Radius, MinShapeSize, 1000.0f);
			const float Length = Clamp(Capsule.Length, MinShapeSize, 1000.0f);
			const float HalfTotalLength = Length * 0.5f + Radius;
			OutSnapshot.Center = Capsule.Center;
			OutSnapshot.Rotation = Capsule.Rotation.GetNormalized();
			OutSnapshot.SphereRadius = Clamp((std::max)(Radius, HalfTotalLength), MinShapeSize, 1000.0f);
			OutSnapshot.BoxExtents = FVector(Radius, Radius, HalfTotalLength);
			OutSnapshot.CapsuleRadius = Radius;
			OutSnapshot.CapsuleLength = Length;
			return true;
		}
		default:
			return false;
		}
	}

	template<typename TEnum>
	bool EnumCombo(const char* Label, TEnum& Value, const char* const* Items, int32 Count)
	{
		int32 Current = static_cast<int32>(Value);
		if (ImGui::Combo(Label, &Current, Items, Count))
		{
			Value = static_cast<TEnum>(Current);
			return true;
		}
		return false;
	}

	bool IsValidAssetPath(const FString& Path)
	{
		return !Path.empty() && Path != "None";
	}

	const TArray<FAssetListItem>& CollectPhysicalMaterialOptions()
	{
		return FAssetRegistry::ListByTypeName("UPhysicalMaterial");
	}

	FString MakePhysicalMaterialDisplayName(const FString& Path)
	{
		if (!IsValidAssetPath(Path))
		{
			return "None";
		}

		const TArray<FAssetListItem>& MaterialOptions = CollectPhysicalMaterialOptions();
		for (const FAssetListItem& Item : MaterialOptions)
		{
			if (Item.FullPath == Path)
			{
				return Item.DisplayName.empty() ? Item.FullPath : Item.DisplayName;
			}
		}

		const size_t Slash = Path.find_last_of("/\\");
		FString Name = Slash == FString::npos ? Path : Path.substr(Slash + 1);
		const size_t Dot = Name.find_last_of('.');
		if (Dot != FString::npos)
		{
			Name = Name.substr(0, Dot);
		}
		return Name.empty() ? Path : Name;
	}

	bool DrawPhysicalMaterialPickerOptions(const FString& CurrentPath, FString& OutSelectedPath)
	{
		bool bSelected = false;

		if (ImGui::Selectable("None", CurrentPath == "None"))
		{
			OutSelectedPath = "None";
			bSelected = true;
		}

		const TArray<FAssetListItem>& MaterialOptions = CollectPhysicalMaterialOptions();
		if (MaterialOptions.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("No UPhysicalMaterial assets found.");
			return bSelected;
		}

		ImGui::Separator();
		ImGui::TextDisabled("UPhysicalMaterial Assets");
		for (const FAssetListItem& Item : MaterialOptions)
		{
			const FString& MaterialPath = Item.FullPath;
			const FString Label = Item.DisplayName.empty() ? Item.FullPath : Item.DisplayName;
			ImGui::PushID(MaterialPath.c_str());
			if (ImGui::Selectable(Label.c_str(), CurrentPath == MaterialPath))
			{
				OutSelectedPath = MaterialPath;
				bSelected = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", MaterialPath.c_str());
			}
			ImGui::PopID();
		}

		return bSelected;
	}

	FString MakeAssetTabName(const FString& SourcePath)
	{
		if (!IsValidAssetPath(SourcePath))
		{
			return "PhysicsAsset";
		}

		const size_t Slash = SourcePath.find_last_of("/\\");
		FString Name = Slash == FString::npos ? SourcePath : SourcePath.substr(Slash + 1);
		const size_t Dot = Name.find_last_of('.');
		if (Dot != FString::npos)
		{
			Name = Name.substr(0, Dot);
		}
		return Name.empty() ? "PhysicsAsset" : Name;
	}

	FVector BoneLocalToWorld(const FMatrix& BoneMatrix, const FVector& LocalPoint)
	{
		return BoneMatrix.TransformPositionWithW(LocalPoint);
	}

	FVector ShapeLocalToBoneLocal(const FVector& Center, const FQuat& Rotation, const FVector& ShapeLocalPoint)
	{
		return Center + Rotation.RotateVector(ShapeLocalPoint);
	}

	EAngularConstraintMode MapAngularMode(EPhysicsAssetConstraintMode In)
	{
		switch (In)
		{
		case EPhysicsAssetConstraintMode::Free:   return EAngularConstraintMode::Free;
		case EPhysicsAssetConstraintMode::Locked: return EAngularConstraintMode::Locked;
		case EPhysicsAssetConstraintMode::Limited:
		default:                                  return EAngularConstraintMode::Limited;
		}
	}

	FQuat MakeQuatFromZToAxis(const FVector& TargetAxis)
	{
		const FVector Z = FVector::ZAxisVector;
		const float Dot = Z.Dot(TargetAxis);
		if (Dot > 0.9999f)
		{
			return FQuat::Identity;
		}
		if (Dot < -0.9999f)
		{
			return FQuat::FromAxisAngle(FVector::XAxisVector, FMath::Pi);
		}

		const FVector Axis = Z.Cross(TargetAxis).Normalized();
		const float Angle = std::acos(std::clamp(Dot, -1.0f, 1.0f));
		return FQuat::FromAxisAngle(Axis, Angle);
	}

	bool MakeDefaultBoneShapeDesc(const FSkeletalMesh& MeshAsset, int32 BoneIndex,
		EPhysicsAssetPrimitiveType PrimitiveType, FPhysicsAssetBodyShapeDesc& OutDesc)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset.Bones.size()))
		{
			return false;
		}

		const FMatrix BoneGlobal = MeshAsset.Bones[BoneIndex].GetReferenceGlobalPose();
		const FMatrix InvBoneGlobal = BoneGlobal.GetInverse();
		FVector Axis = FVector::ZAxisVector;
		float BoneLength = 0.35f;

		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(MeshAsset.Bones.size()); ++ChildIndex)
		{
			if (MeshAsset.Bones[ChildIndex].ParentIndex != BoneIndex)
			{
				continue;
			}

			const FVector ChildLocal = InvBoneGlobal.TransformPositionWithW(
				MeshAsset.Bones[ChildIndex].GetReferenceGlobalPose().GetLocation());
			const float ChildLength = ChildLocal.Length();
			if (ChildLength > BoneLength)
			{
				BoneLength = ChildLength;
				Axis = ChildLocal / ChildLength;
			}
		}

		const float MinExtent = 0.025f;
		const float ScaledBoneLength = BoneLength * 0.9f;
		const float MinimumLength = MinExtent * 4.0f;
		const float TargetTotalLength = ScaledBoneLength > MinimumLength ? ScaledBoneLength : MinimumLength;
		const float Radius = Clamp(TargetTotalLength * 0.12f, MinExtent, TargetTotalLength * 0.25f);
		const FVector Center = Axis * (BoneLength * 0.5f);
		const FQuat Rotation = MakeQuatFromZToAxis(Axis);

		switch (PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Box:
			OutDesc = FPhysicsAssetBodyShapeDesc::MakeBox(Center, Rotation, FVector(Radius, Radius, TargetTotalLength * 0.5f));
			break;
		case EPhysicsAssetPrimitiveType::Sphere:
		{
			const float SphereRadius = Radius > TargetTotalLength * 0.25f ? Radius : TargetTotalLength * 0.25f;
			OutDesc = FPhysicsAssetBodyShapeDesc::MakeSphere(Center, SphereRadius);
			break;
		}
		case EPhysicsAssetPrimitiveType::Capsule:
		default:
		{
			const float CylinderLength = TargetTotalLength - Radius * 2.0f;
			OutDesc = FPhysicsAssetBodyShapeDesc::MakeCapsule(Center, Rotation, Radius, CylinderLength > MinExtent ? CylinderLength : MinExtent);
			break;
		}
		}
		return true;
	}

	bool BoneSubtreeMatchesFilter(const FSkeletalMesh* MeshAsset, int32 BoneIndex, const FString& Filter)
	{
		if (Filter.empty())
		{
			return true;
		}
		if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
		{
			return false;
		}

		const FBone& Bone = MeshAsset->Bones[BoneIndex];
		if (Bone.Name.find(Filter) != FString::npos)
		{
			return true;
		}

		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(MeshAsset->Bones.size()); ++ChildIndex)
		{
			if (MeshAsset->Bones[ChildIndex].ParentIndex == BoneIndex
				&& BoneSubtreeMatchesFilter(MeshAsset, ChildIndex, Filter))
			{
				return true;
			}
		}
		return false;
	}

	void DrawPhysicsPanelHeader(const char* Label)
	{
		constexpr float HeaderHeight = 28.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + HeaderHeight), IM_COL32(44, 44, 44, 255));
		DrawList->AddLine(Pos, ImVec2(Pos.x + Width, Pos.y), IM_COL32(70, 70, 70, 255));
		DrawList->AddLine(ImVec2(Pos.x, Pos.y + HeaderHeight - 1.0f),
			ImVec2(Pos.x + Width, Pos.y + HeaderHeight - 1.0f), IM_COL32(20, 20, 20, 255));
		DrawList->AddText(ImVec2(Pos.x + 9.0f, Pos.y + 6.0f), IM_COL32(218, 218, 218, 255), Label);
		ImGui::Dummy(ImVec2(Width, HeaderHeight + 6.0f));
	}

	bool DrawSmallToolbarButton(const char* Label, const ImVec2& Size, bool bActive = false)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, bActive ? ImVec4(0.10f, 0.34f, 0.68f, 1.0f) : ImVec4(0.19f, 0.19f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bActive ? ImVec4(0.14f, 0.42f, 0.80f, 1.0f) : ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, bActive ? ImVec4(0.08f, 0.28f, 0.58f, 1.0f) : ImVec4(0.31f, 0.31f, 0.31f, 1.0f));
		const bool bPressed = ImGui::Button(Label, Size);
		ImGui::PopStyleColor(3);
		return bPressed;
	}

	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		return Value;
	}

	bool DetailFilterMatches(const FString& Filter, const FString& Label)
	{
		if (Filter.empty())
		{
			return true;
		}
		return ToLowerCopy(Label).find(ToLowerCopy(Filter)) != FString::npos;
	}

	bool BeginPhysicsDetailsTable(const char* Id)
	{
		const ImGuiTableFlags Flags =
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_PadOuterX |
			ImGuiTableFlags_RowBg;
		if (!ImGui::BeginTable(Id, 2, Flags))
		{
			return false;
		}

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 168.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));
		return true;
	}

	void EndPhysicsDetailsTable()
	{
		ImGui::EndTable();
		ImGui::PopStyleColor(2);
	}

	void DrawDetailsLabel(const char* Label)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
		ImGui::TextUnformatted(Label);
		ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
	}

	void DrawDetailsTextRow(const char* Label, const FString& Value, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextWrapped("%s", Value.c_str());
	}

	bool DrawDetailsStringRow(const char* Label, FString& Value, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return false;
		}

		char Buffer[512] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%s", Value.c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushID(Label);
		const bool bChanged = ImGui::InputText("##Value", Buffer, sizeof(Buffer));
		ImGui::PopID();
		if (bChanged)
		{
			Value = Buffer;
		}
		return bChanged;
	}

	bool DrawDetailsBoolRow(const char* Label, bool& Value, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return false;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushID(Label);
		const bool bChanged = ImGui::Checkbox("##Value", &Value);
		ImGui::PopID();
		return bChanged;
	}

	bool DrawDetailsFloatRow(const char* Label, float& Value, float Speed, float Min, float Max, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return false;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushID(Label);
		bool bChanged = false;
		if (Min != 0.0f || Max != 0.0f)
		{
			bChanged = ImGui::DragFloat("##Value", &Value, Speed, Min, Max, "%.4f");
		}
		else
		{
			bChanged = ImGui::DragFloat("##Value", &Value, Speed);
		}
		ImGui::PopID();
		return bChanged;
	}

	bool DrawDetailsVec3Row(const char* Label, FVector& Value, float Speed, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return false;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushID(Label);
		const bool bChanged = ImGui::DragFloat3("##Value", &Value.X, Speed);
		ImGui::PopID();
		return bChanged;
	}

	bool DrawDetailsEnumRow(const char* Label, int32& Value, const char* const* Items, int32 Count, const FString& Filter)
	{
		if (!DetailFilterMatches(Filter, Label))
		{
			return false;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		DrawDetailsLabel(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushID(Label);
		const bool bChanged = ImGui::Combo("##Value", &Value, Items, Count);
		ImGui::PopID();
		return bChanged;
	}

	bool DrawPhysicsSplitter(const char* Id, const ImVec2& Size, bool bVertical)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::Button(Id, Size);
		const bool bActive = ImGui::IsItemActive();
		if (ImGui::IsItemHovered() || bActive)
		{
			ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
		}
		ImGui::PopStyleColor(3);
		return bActive;
	}
}

FPhysicsAssetEditorWidget::FPhysicsAssetEditorWidget()
	: InstanceId(GNextPhysicsAssetEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("PhysicsAssetEditorPreview_" + Id);
	WindowIdSuffix = "###PhysicsAssetEditor_" + Id;
}

bool FPhysicsAssetEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UPhysicsAsset>();
}

bool FPhysicsAssetEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UPhysicsAsset* CurrentAsset = Cast<UPhysicsAsset>(EditedObject);
	const UPhysicsAsset* RequestedAsset = Cast<UPhysicsAsset>(Object);
	return IsOpen()
		&& CurrentAsset
		&& RequestedAsset
		&& IsValidAssetPath(CurrentAsset->GetSourcePath())
		&& CurrentAsset->GetSourcePath() == RequestedAsset->GetSourcePath();
}

void FPhysicsAssetEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	ClearSelection();
	EnsureGraphEditorContext();
	bGraphPositionsPushed = false;

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(EditedObject);
	PreviewMesh = nullptr;
	PreviewMeshComponent = nullptr;
	PreviewActor = nullptr;

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (PhysicsAsset && IsValidAssetPath(PhysicsAsset->GetPreviewSkeletalMeshPath()))
	{
		PreviewMesh = FMeshManager::LoadSkeletalMesh(PhysicsAsset->GetPreviewSkeletalMeshPath(), Device);
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();
	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	if (PreviewMesh)
	{
		PreviewMeshComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
		PreviewMeshComponent->SetSkeletalMesh(PreviewMesh);
		PreviewMeshComponent->SetPhysicsAssetOverride(PhysicsAsset);
		PreviewActor->SetRootComponent(PreviewMeshComponent);
	}
	PreviewActor->SetActorLocation(FVector::ZeroVector);

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	AActor* FloorCollisionActor = WorldContext.World->SpawnActor<AActor>();
	UBoxComponent* FloorCollision = FloorCollisionActor ? FloorCollisionActor->AddComponent<UBoxComponent>() : nullptr;
	if (FloorCollisionActor && FloorCollision)
	{
		FloorCollisionActor->SetRootComponent(FloorCollision);
		FloorCollision->SetBoxExtent(FVector(10.0f, 10.0f, 0.1f));
		FloorCollisionActor->SetActorLocation(FVector(0.0f, 0.0f, -0.1f));
		FloorCollision->SetVisibility(false);
		FloorCollision->SetCollisionObjectType(ECollisionChannel::WorldStatic);
		FloorCollision->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
		FloorCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	ViewportClient.Initialize(Device, static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewScene(WorldContext.World, PhysicsAsset, PreviewMeshComponent);
	ViewportClient.CreatePreviewGizmo();
	ApplyViewPreset(ActiveViewPreset);
	ViewportClient.SetSimulatePhysics(false);
	ViewportClient.ResetCameraToPreviewBounds();
	bPreviewSimulationActive = false;

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

bool FPhysicsAssetEditorWidget::ReloadPreviewMeshFromAsset(UPhysicsAsset* Asset)
{
	if (!Asset || !PreviewActor)
	{
		return false;
	}

	StopPreviewSimulation();

	USkeletalMesh* NewPreviewMesh = nullptr;
	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (IsValidAssetPath(Asset->GetPreviewSkeletalMeshPath()))
	{
		NewPreviewMesh = FMeshManager::LoadSkeletalMesh(Asset->GetPreviewSkeletalMeshPath(), Device);
	}

	if (!NewPreviewMesh)
	{
		if (PreviewMeshComponent)
		{
			PreviewMeshComponent->SetVisibility(false);
		}
		PreviewMesh = nullptr;
		PreviewMeshComponent = nullptr;
		ViewportClient.SetPreviewScene(ViewportClient.GetPreviewWorld(), Asset, nullptr);
		ClearSelection();
		return false;
	}

	PreviewMesh = NewPreviewMesh;
	if (!PreviewMeshComponent)
	{
		PreviewMeshComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
		PreviewActor->SetRootComponent(PreviewMeshComponent);
	}

	PreviewMeshComponent->SetSkeletalMesh(PreviewMesh);
	PreviewMeshComponent->SetPhysicsAssetOverride(Asset);
	PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
	ViewportClient.SetPreviewScene(ViewportClient.GetPreviewWorld(), Asset, PreviewMeshComponent);
	ViewportClient.ResetCameraToPreviewBounds();
	ClearSelection();
	bGraphPositionsPushed = false;
	return true;
}

void FPhysicsAssetEditorWidget::Close()
{
	FAssetEditorWidget::Close();
	StopPreviewSimulation();
	DestroyGraphEditorContext();

	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();
	if (PreviewWorld)
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();

	if (PreviewWorldHandle.IsValid())
	{
		GEngine->DestroyWorldContext(PreviewWorldHandle);
	}

	PreviewMesh = nullptr;
	PreviewMeshComponent = nullptr;
	PreviewActor = nullptr;
	ClearSelection();
}

void FPhysicsAssetEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
		if (ViewportClient.ConsumeGizmoEdited())
		{
			MarkDirty();
		}
	}
	TickPreviewSimulation(DeltaTime);
}

void FPhysicsAssetEditorWidget::TickPreviewSimulation(float DeltaTime)
{
	const bool bShouldSimulate = ViewportClient.IsSimulatingPhysics();
	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();

	if (!PreviewWorld || !PreviewMeshComponent)
	{
		bPreviewSimulationActive = false;
		ViewportClient.SetSimulatePhysics(false);
		return;
	}

	if (!bShouldSimulate)
	{
		StopPreviewSimulation();
		return;
	}

	if (!bPreviewSimulationActive)
	{
		CapturePreviewSimulationStartPose();
		PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
		PreviewMeshComponent->SetSimulatePhysics(true);
		bPreviewSimulationActive = PreviewMeshComponent->IsSimulatingPhysics();

		if (!bPreviewSimulationActive)
		{
			PreviewSimulationStartLocalPose.clear();
			PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
			ViewportClient.SetSimulatePhysics(false);
			return;
		}
	}

	PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
	const float SimDeltaTime = (std::max)(0.0f, DeltaTime);
	PreviewWorld->Tick(SimDeltaTime, ELevelTick::LEVELTICK_All);
	PreviewMeshComponent->SyncSimulatedPhysics();
}

void FPhysicsAssetEditorWidget::CapturePreviewSimulationStartPose()
{
	PreviewSimulationStartLocalPose.clear();

	if (!PreviewMeshComponent)
	{
		return;
	}

	USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		return;
	}

	const int32 NumBones = static_cast<int32>(MeshAsset->Bones.size());
	PreviewSimulationStartLocalPose.reserve(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		PreviewSimulationStartLocalPose.push_back(PreviewMeshComponent->GetBoneLocalTransformByIndex(BoneIndex));
	}
}

void FPhysicsAssetEditorWidget::RestorePreviewSimulationStartPose()
{
	if (!PreviewMeshComponent || PreviewSimulationStartLocalPose.empty())
	{
		PreviewSimulationStartLocalPose.clear();
		return;
	}

	USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const int32 NumBones = MeshAsset ? static_cast<int32>(MeshAsset->Bones.size()) : 0;
	if (NumBones <= 0 || static_cast<int32>(PreviewSimulationStartLocalPose.size()) != NumBones)
	{
		PreviewSimulationStartLocalPose.clear();
		return;
	}

	PreviewMeshComponent->SetBoneLocalTransforms(PreviewSimulationStartLocalPose);
	PreviewSimulationStartLocalPose.clear();
}

void FPhysicsAssetEditorWidget::StopPreviewSimulation()
{
	if (PreviewMeshComponent && PreviewMeshComponent->IsSimulatingPhysics())
	{
		PreviewMeshComponent->SetSimulatePhysics(false);
	}
	RestorePreviewSimulationStartPose();

	bPreviewSimulationActive = false;
	ViewportClient.SetSimulatePhysics(false);
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
	}
}

void FPhysicsAssetEditorWidget::EnsureGraphEditorContext()
{
	if (GraphEditorContext)
	{
		return;
	}

	ed::Config Config;
	Config.SettingsFile = nullptr;
	GraphEditorContext = ed::CreateEditor(&Config);
}

void FPhysicsAssetEditorWidget::DestroyGraphEditorContext()
{
	if (!GraphEditorContext)
	{
		return;
	}

	ed::DestroyEditor(GraphEditorContext);
	GraphEditorContext = nullptr;
	bGraphPositionsPushed = false;
	bSelectionOriginatedFromGraph = false;
	bPendingGraphSelectionSync = false;
	bSuppressNextGraphSelectionEvent = false;
}

void FPhysicsAssetEditorWidget::ClearGraphEditorSelection()
{
	if (!GraphEditorContext)
	{
		return;
	}

	ed::EditorContext* PreviousContext = ed::GetCurrentEditor();
	ed::SetCurrentEditor(GraphEditorContext);
	ed::ClearSelection();
	ed::SetCurrentEditor(PreviousContext);
	bPendingGraphSelectionSync = false;
	bSuppressNextGraphSelectionEvent = false;
}

void FPhysicsAssetEditorWidget::RequestGraphEditorSelectionSync()
{
	bPendingGraphSelectionSync = true;
}

void FPhysicsAssetEditorWidget::SyncGraphEditorSelectionToSelection()
{
	if (!GraphEditorContext)
	{
		bPendingGraphSelectionSync = false;
		return;
	}

	ed::EditorContext* PreviousContext = ed::GetCurrentEditor();
	ed::SetCurrentEditor(GraphEditorContext);
	ed::ClearSelection();

	switch (Selection.Type)
	{
	case EPhysicsAssetEditorSelectionType::Body:
	case EPhysicsAssetEditorSelectionType::Shape:
		if (Selection.BodyIndex >= 0)
		{
			ed::SelectNode(ToPhysicsGraphNodeId(Selection.BodyIndex));
		}
		break;
	case EPhysicsAssetEditorSelectionType::Constraint:
		if (Selection.ConstraintIndex >= 0)
		{
			ed::SelectNode(ToPhysicsGraphConstraintNodeId(Selection.ConstraintIndex));
			ed::SelectLink(ToPhysicsGraphLinkId(Selection.ConstraintIndex), true);
			ed::SelectLink(ToPhysicsGraphLinkedBodyLinkId(Selection.ConstraintIndex), true);
		}
		break;
	default:
		break;
	}

	ed::SetCurrentEditor(PreviousContext);
	bPendingGraphSelectionSync = false;
	bSuppressNextGraphSelectionEvent = true;
}

bool FPhysicsAssetEditorWidget::GetSelectedBodyBoneName(UPhysicsAsset* Asset, FName& OutBoneName) const
{
	OutBoneName = FName::None;
	if (!Asset)
	{
		return false;
	}

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	auto ResolveBodyIndex = [&](int32 BodyIndex) -> bool
	{
		if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[BodyIndex])
		{
			return false;
		}
		OutBoneName = Bodies[BodyIndex]->GetBoneName();
		return OutBoneName.IsValid() && OutBoneName != FName::None;
	};

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body
		|| Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
	{
		return ResolveBodyIndex(Selection.BodyIndex);
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone && PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
	{
		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (Selection.BoneIndex >= 0 && Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size()))
		{
			return ResolveBodyIndex(Asset->FindBodyIndex(FName(MeshAsset->Bones[Selection.BoneIndex].Name)));
		}
	}

	return false;
}

bool FPhysicsAssetEditorWidget::SetSelectedCollisionEnabled(UPhysicsAsset* Asset, bool bEnabled)
{
	if (!Asset)
	{
		return false;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
	{
		const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
		if (Selection.ConstraintIndex < 0
			|| Selection.ConstraintIndex >= static_cast<int32>(Constraints.size())
			|| !Constraints[Selection.ConstraintIndex])
		{
			return false;
		}

		UPhysicsConstraintTemplate* Constraint = Constraints[Selection.ConstraintIndex];
		const bool bCurrentDisabled = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
		const bool bTargetDisabled = !bEnabled;
		if (bCurrentDisabled == bTargetDisabled)
		{
			return false;
		}
		Asset->SetCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName(), bTargetDisabled);
		return true;
	}

	FName BoneName = FName::None;
	if (!GetSelectedBodyBoneName(Asset, BoneName))
	{
		return false;
	}

	const bool bCurrentEnabled = Asset->IsBodyCollisionEnabled(BoneName);
	if (bCurrentEnabled == bEnabled)
	{
		return false;
	}
	Asset->SetBodyCollisionEnabled(BoneName, bEnabled);
	return true;
}

bool FPhysicsAssetEditorWidget::SetSelectedPhysicalMaterialPath(UPhysicsAsset* Asset, const FString& Path)
{
	FName BoneName = FName::None;
	if (!GetSelectedBodyBoneName(Asset, BoneName))
	{
		return false;
	}

	const FString NormalizedPath = Path.empty() ? "None" : Path;
	if (Asset->GetBodyPhysicalMaterialPath(BoneName) == NormalizedPath)
	{
		return false;
	}

	Asset->SetBodyPhysicalMaterialPath(BoneName, NormalizedPath);
	return true;
}

bool FPhysicsAssetEditorWidget::ApplyConstraintPreset(UPhysicsAsset* Asset, int32 ConstraintIndex, EAngularConstraintMode Mode, float Swing1, float Swing2, float Twist)
{
	if (!Asset)
	{
		return false;
	}

	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(Constraints.size()) || !Constraints[ConstraintIndex])
	{
		return false;
	}

	UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
	const bool bChanged = Constraint->GetAngularMode() != Mode
		|| std::fabs(Constraint->GetSwing1Limit() - Swing1) > 0.001f
		|| std::fabs(Constraint->GetSwing2Limit() - Swing2) > 0.001f
		|| std::fabs(Constraint->GetTwistLimit() - Twist) > 0.001f;
	if (!bChanged)
	{
		return false;
	}

	Constraint->SetAngularMode(Mode);
	Constraint->SetAngularLimits(Swing1, Swing2, Twist);
	return true;
}

UPhysicsConstraintTemplate* FPhysicsAssetEditorWidget::CreateConstraintBetweenBodies(UPhysicsAsset* Asset, int32 ParentBodyIndex, int32 ChildBodyIndex)
{
	if (!Asset)
	{
		return nullptr;
	}

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	if (ParentBodyIndex < 0 || ChildBodyIndex < 0
		|| ParentBodyIndex >= static_cast<int32>(Bodies.size())
		|| ChildBodyIndex >= static_cast<int32>(Bodies.size())
		|| !Bodies[ParentBodyIndex]
		|| !Bodies[ChildBodyIndex])
	{
		return nullptr;
	}

	const FName ParentBone = Bodies[ParentBodyIndex]->GetBoneName();
	const FName ChildBone = Bodies[ChildBodyIndex]->GetBoneName();
	FTransform FrameA;
	FTransform FrameB;

	if (PreviewMeshComponent)
	{
		FMatrix ParentMatrix;
		FMatrix ChildMatrix;
		if (PreviewMeshComponent->GetBoneWorldMatrixByName(ParentBone.ToString(), ParentMatrix)
			&& PreviewMeshComponent->GetBoneWorldMatrixByName(ChildBone.ToString(), ChildMatrix))
		{
			const FVector JointWorld = ChildMatrix.GetLocation();
			FrameA.Location = ParentMatrix.GetInverse().TransformPositionWithW(JointWorld);
			FrameB.Location = ChildMatrix.GetInverse().TransformPositionWithW(JointWorld);
			FrameA.Rotation = FQuat::Identity;
			FrameB.Rotation = FQuat::Identity;
			FrameA.Scale = FVector::OneVector;
			FrameB.Scale = FVector::OneVector;
		}
	}

	UPhysicsConstraintTemplate* Constraint = Asset->CreateConstraint(
		ParentBone,
		ChildBone,
		FrameA,
		FrameB,
		EAngularConstraintMode::Limited);
	if (Constraint)
	{
		Constraint->SetAngularLimits(45.0f, 45.0f, 30.0f);
	}
	return Constraint;
}

void FPhysicsAssetEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FPhysicsAssetEditorViewportClient*>(&ViewportClient));
	}
}

void FPhysicsAssetEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(EditedObject);
	if (!PhysicsAsset)
	{
		return;
	}
	ValidateSelection(PhysicsAsset);

	FString VisibleTitle = "Physics Asset Editor";
	if (IsValidAssetPath(PhysicsAsset->GetSourcePath()))
	{
		VisibleTitle += " - ";
		VisibleTitle += PhysicsAsset->GetSourcePath();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	bool bWindowOpen = true;
	const FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGui::SetNextWindowSize(ImVec2(1520.0f, 900.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	int32 StyleVarCount = 0;
	int32 StyleColorCount = 0;
	auto PushVar = [&](ImGuiStyleVar Var, const ImVec2& Value)
	{
		ImGui::PushStyleVar(Var, Value);
		++StyleVarCount;
	};
	auto PushVarFloat = [&](ImGuiStyleVar Var, float Value)
	{
		ImGui::PushStyleVar(Var, Value);
		++StyleVarCount;
	};
	auto PushColor = [&](ImGuiCol Color, const ImVec4& Value)
	{
		ImGui::PushStyleColor(Color, Value);
		++StyleColorCount;
	};

	PushVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
	PushVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
	PushVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
	PushVarFloat(ImGuiStyleVar_WindowRounding, 0.0f);
	PushVarFloat(ImGuiStyleVar_ChildRounding, 0.0f);
	PushVarFloat(ImGuiStyleVar_FrameRounding, 2.0f);
	PushVarFloat(ImGuiStyleVar_TabRounding, 0.0f);
	PushVarFloat(ImGuiStyleVar_GrabRounding, 1.0f);

	PushColor(ImGuiCol_WindowBg, ImVec4(0.105f, 0.105f, 0.105f, 1.0f));
	PushColor(ImGuiCol_ChildBg, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));
	PushColor(ImGuiCol_Border, ImVec4(0.035f, 0.035f, 0.035f, 1.0f));
	PushColor(ImGuiCol_FrameBg, ImVec4(0.045f, 0.045f, 0.045f, 1.0f));
	PushColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
	PushColor(ImGuiCol_FrameBgActive, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	PushColor(ImGuiCol_Header, ImVec4(0.08f, 0.28f, 0.52f, 1.0f));
	PushColor(ImGuiCol_HeaderHovered, ImVec4(0.10f, 0.36f, 0.68f, 1.0f));
	PushColor(ImGuiCol_HeaderActive, ImVec4(0.07f, 0.30f, 0.58f, 1.0f));
	PushColor(ImGuiCol_Button, ImVec4(0.19f, 0.19f, 0.19f, 1.0f));
	PushColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
	PushColor(ImGuiCol_ButtonActive, ImVec4(0.31f, 0.31f, 0.31f, 1.0f));
	PushColor(ImGuiCol_Tab, ImVec4(0.17f, 0.17f, 0.17f, 1.0f));
	PushColor(ImGuiCol_TabHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
	PushColor(ImGuiCol_TabSelected, ImVec4(0.12f, 0.29f, 0.50f, 1.0f));
	PushColor(ImGuiCol_TabDimmed, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	PushColor(ImGuiCol_CheckMark, ImVec4(0.03f, 0.48f, 0.95f, 1.0f));

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		ImGui::PopStyleColor(StyleColorCount);
		ImGui::PopStyleVar(StyleVarCount);
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderModeToolbar(PhysicsAsset);

	const ImGuiStyle& Style = ImGui::GetStyle();
	constexpr float SplitterThickness = 4.0f;
	const float TotalHeight = ImGui::GetContentRegionAvail().y;
	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	constexpr float MinSkeletonTreeHeight = 120.0f;
	constexpr float MinGraphPanelHeight = 220.0f;

	float MaxHierarchyWidth = TotalWidth - DetailsWidth - 520.0f;
	if (MaxHierarchyWidth < 240.0f) MaxHierarchyWidth = 240.0f;
	if (MaxHierarchyWidth > 420.0f) MaxHierarchyWidth = 420.0f;
	HierarchyWidth = Clamp(HierarchyWidth, 260.0f, MaxHierarchyWidth);

	float MaxDetailsWidth = TotalWidth - HierarchyWidth - 520.0f;
	if (MaxDetailsWidth < 280.0f) MaxDetailsWidth = 280.0f;
	if (MaxDetailsWidth > 420.0f) MaxDetailsWidth = 420.0f;
	DetailsWidth = Clamp(DetailsWidth, 320.0f, MaxDetailsWidth);

	ImGui::BeginChild("##PhysicsAssetLeftColumn", ImVec2(HierarchyWidth, TotalHeight), false);
	const float LeftSplitterGap = SplitterThickness + Style.ItemSpacing.y;
	const float LeftContentHeight = (std::max)(0.0f, TotalHeight - LeftSplitterGap);
	float SkeletonTreeHeight = 0.0f;
	if (LeftContentHeight <= MinSkeletonTreeHeight + MinGraphPanelHeight)
	{
		GraphHeight = (std::max)(140.0f, LeftContentHeight * 0.45f);
		GraphHeight = (std::min)(GraphHeight, (std::max)(0.0f, LeftContentHeight - 60.0f));
		SkeletonTreeHeight = (std::max)(0.0f, LeftContentHeight - GraphHeight);
	}
	else
	{
		const float MaxGraphHeight = LeftContentHeight - MinSkeletonTreeHeight;
		GraphHeight = Clamp(GraphHeight, MinGraphPanelHeight, MaxGraphHeight);
		SkeletonTreeHeight = LeftContentHeight - GraphHeight;
	}
	ImGui::BeginChild("##PhysicsAssetSkeletonTree", ImVec2(0.0f, SkeletonTreeHeight), true);
	RenderSkeletonTreePanel(PhysicsAsset);
	ImGui::EndChild();
	if (DrawPhysicsSplitter("##PhysicsAssetLeftHorizontalSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		GraphHeight -= ImGui::GetIO().MouseDelta.y;
		const float MaxGraphHeight = LeftContentHeight > MinSkeletonTreeHeight
			? LeftContentHeight - MinSkeletonTreeHeight
			: LeftContentHeight;
		GraphHeight = Clamp(GraphHeight, (std::min)(MinGraphPanelHeight, LeftContentHeight), MaxGraphHeight);
	}
	RenderGraphPanel(PhysicsAsset, ImVec2(0.0f, GraphHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	if (DrawPhysicsSplitter("##PhysicsAssetTreeSplitter", ImVec2(SplitterThickness, TotalHeight), true))
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		float MaxWidth = ImGui::GetWindowWidth() - DetailsWidth - 520.0f;
		if (MaxWidth < 240.0f) MaxWidth = 240.0f;
		if (MaxWidth > 420.0f) MaxWidth = 420.0f;
		HierarchyWidth = Clamp(HierarchyWidth, 260.0f, MaxWidth);
	}

	ImGui::SameLine();

	float CenterWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - Style.ItemSpacing.x - SplitterThickness;
	if (CenterWidth < 320.0f) CenterWidth = 320.0f;
	ImGui::BeginChild("##PhysicsAssetCenterColumn", ImVec2(CenterWidth, TotalHeight), false);
	const float CenterHeight = ImGui::GetContentRegionAvail().y;
	const float CenterContentWidth = ImGui::GetContentRegionAvail().x;
	float MaxListHeight = CenterHeight - 260.0f;
	if (MaxListHeight < 150.0f) MaxListHeight = 150.0f;
	ViewportListHeight = Clamp(ViewportListHeight, 150.0f, MaxListHeight);
	const float ViewportHeight = CenterHeight - ViewportListHeight - SplitterThickness - Style.ItemSpacing.y;
	RenderViewportPanel(PhysicsAsset, ImVec2(CenterContentWidth, ViewportHeight));
	if (DrawPhysicsSplitter("##PhysicsAssetViewportListSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ViewportListHeight -= ImGui::GetIO().MouseDelta.y;
		ViewportListHeight = Clamp(ViewportListHeight, 150.0f, MaxListHeight);
	}
	RenderPhysicsListPanel(PhysicsAsset, ImVec2(CenterContentWidth, ViewportListHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	if (DrawPhysicsSplitter("##PhysicsAssetDetailsSplitter", ImVec2(SplitterThickness, TotalHeight), true))
	{
		DetailsWidth -= ImGui::GetIO().MouseDelta.x;
		float MaxWidth = ImGui::GetWindowWidth() - HierarchyWidth - 520.0f;
		if (MaxWidth < 280.0f) MaxWidth = 280.0f;
		if (MaxWidth > 420.0f) MaxWidth = 420.0f;
		DetailsWidth = Clamp(DetailsWidth, 320.0f, MaxWidth);
	}

	ImGui::SameLine();

	ImGui::BeginChild("##PhysicsAssetRightColumn", ImVec2(DetailsWidth, TotalHeight), false);
	float MaxToolsHeight = TotalHeight - 260.0f;
	if (MaxToolsHeight < 160.0f) MaxToolsHeight = 160.0f;
	ToolsHeight = Clamp(ToolsHeight, 160.0f, MaxToolsHeight);
	const float DetailsPanelHeight = TotalHeight - ToolsHeight - SplitterThickness - Style.ItemSpacing.y;
	ImGui::BeginChild("##PhysicsAssetDetails", ImVec2(0.0f, DetailsPanelHeight), true);
	RenderDetailsPanel(PhysicsAsset);
	ImGui::EndChild();
	if (DrawPhysicsSplitter("##PhysicsAssetDetailsToolsSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ToolsHeight -= ImGui::GetIO().MouseDelta.y;
		ToolsHeight = Clamp(ToolsHeight, 160.0f, MaxToolsHeight);
	}
	RenderToolsPanel(PhysicsAsset, ImVec2(0.0f, ToolsHeight));
	ImGui::EndChild();

	ImGui::End();
	ImGui::PopStyleColor(StyleColorCount);
	ImGui::PopStyleVar(StyleVarCount);

	if (!bWindowOpen || bPendingClose)
	{
		bPendingClose = false;
		Close();
	}
}

void FPhysicsAssetEditorWidget::RenderModeToolbar(UPhysicsAsset* Asset)
{
	constexpr float TabHeight = 34.0f;
	constexpr float ToolbarHeight = 42.0f;
	constexpr float BarHeight = TabHeight + ToolbarHeight;
	const ImVec2 BarPos = ImGui::GetCursorScreenPos();
	const float BarWidth = ImGui::GetContentRegionAvail().x;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImGui::Dummy(ImVec2(BarWidth, BarHeight));

	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight), IM_COL32(30, 30, 30, 255));
	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + TabHeight), IM_COL32(21, 21, 21, 255));
	DrawList->AddLine(ImVec2(BarPos.x, BarPos.y + TabHeight), ImVec2(BarPos.x + BarWidth, BarPos.y + TabHeight), IM_COL32(3, 3, 3, 255));
	DrawList->AddLine(ImVec2(BarPos.x, BarPos.y + BarHeight - 1.0f), ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight - 1.0f), IM_COL32(5, 5, 5, 255));

	const FString TabName = MakeAssetTabName(Asset ? Asset->GetSourcePath() : FString());
	const ImVec2 TabMin(BarPos.x + 6.0f, BarPos.y + 5.0f);
	const ImVec2 TabMax(BarPos.x + 270.0f, BarPos.y + TabHeight);
	DrawList->AddRectFilled(TabMin, TabMax, IM_COL32(38, 38, 38, 255), 0.0f);
	DrawList->AddRect(TabMin, TabMax, IM_COL32(8, 8, 8, 255), 0.0f);
	DrawList->AddCircleFilled(ImVec2(TabMin.x + 15.0f, TabMin.y + 12.0f), 5.0f, IM_COL32(246, 166, 83, 255));
	DrawList->AddText(ImVec2(TabMin.x + 28.0f, TabMin.y + 5.0f), IM_COL32(230, 230, 230, 255), TabName.c_str());
	DrawList->AddText(ImVec2(TabMax.x - 22.0f, TabMin.y + 5.0f), IM_COL32(145, 145, 145, 255), "x");

	constexpr float ButtonHeight = 27.0f;
	constexpr float RowPaddingX = 8.0f;
	constexpr float ButtonSpacing = 4.0f;
	float CursorX = BarPos.x + RowPaddingX;
	float CursorY = BarPos.y + TabHeight + 7.0f;
	float CursorLimitX = BarPos.x + BarWidth - RowPaddingX;

	auto HasToolbarRoom = [&](float Width)
	{
		return CursorX + Width <= CursorLimitX;
	};
	auto AdvanceToolbar = [&](float Width)
	{
		CursorX += Width + ButtonSpacing;
	};
	auto DrawSeparator = [&]()
	{
		constexpr float SeparatorWidth = 10.0f;
		if (!HasToolbarRoom(SeparatorWidth))
		{
			return;
		}
		DrawList->AddLine(ImVec2(CursorX + 4.0f, CursorY + 2.0f),
			ImVec2(CursorX + 4.0f, CursorY + 26.0f), IM_COL32(10, 10, 10, 255));
		AdvanceToolbar(SeparatorWidth);
	};
	auto DrawToolbarButton = [&](const char* Label, float Width, bool bActive = false)
	{
		if (!HasToolbarRoom(Width))
		{
			return false;
		}
		ImGui::SetCursorScreenPos(ImVec2(CursorX, CursorY));
		const bool bPressed = DrawSmallToolbarButton(Label, ImVec2(Width, ButtonHeight), bActive);
		AdvanceToolbar(Width);
		return bPressed;
	};
	auto DrawToolbarDropdownButton = [&](const char* Label, const char* Id, float Width, bool bActive = false)
	{
		if (!HasToolbarRoom(Width))
		{
			return false;
		}

		ImGui::SetCursorScreenPos(ImVec2(CursorX, CursorY));
		ImGui::PushID(Id);
		const ImVec2 Size(Width, ButtonHeight);
		const bool bPressed = ImGui::InvisibleButton("##DropdownButton", Size);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		ImDrawList* ButtonDrawList = ImGui::GetWindowDrawList();

		const ImU32 FillColor = bActive
			? (bHeld ? IM_COL32(20, 72, 148, 255) : (bHovered ? IM_COL32(36, 107, 204, 255) : IM_COL32(26, 87, 174, 255)))
			: (bHeld ? IM_COL32(79, 79, 79, 255) : (bHovered ? IM_COL32(66, 66, 66, 255) : IM_COL32(49, 49, 49, 255)));
		ButtonDrawList->AddRectFilled(Min, Max, FillColor, 2.0f);
		ButtonDrawList->AddRect(Min, Max, IM_COL32(12, 12, 12, 255), 2.0f);

		const float ArrowRegionWidth = 24.0f;
		const float SplitX = Max.x - ArrowRegionWidth;
		ButtonDrawList->AddLine(ImVec2(SplitX, Min.y + 4.0f), ImVec2(SplitX, Max.y - 4.0f), IM_COL32(18, 18, 18, 180));
		ButtonDrawList->AddText(ImVec2(Min.x + 10.0f, Min.y + 6.0f), IM_COL32(230, 230, 230, 255), Label);

		const float ArrowCenterX = SplitX + ArrowRegionWidth * 0.5f;
		const float ArrowCenterY = Min.y + Size.y * 0.5f + 1.0f;
		ButtonDrawList->AddTriangleFilled(
			ImVec2(ArrowCenterX - 4.0f, ArrowCenterY - 2.0f),
			ImVec2(ArrowCenterX + 4.0f, ArrowCenterY - 2.0f),
			ImVec2(ArrowCenterX, ArrowCenterY + 3.0f),
			IM_COL32(220, 220, 220, 255));
		ImGui::PopID();
		AdvanceToolbar(Width);
		return bPressed;
	};
	auto DrawDisabledToolbarButton = [&](const char* Label, float Width)
	{
		if (!HasToolbarRoom(Width))
		{
			return;
		}
		ImGui::SetCursorScreenPos(ImVec2(CursorX, CursorY));
		ImGui::BeginDisabled();
		DrawSmallToolbarButton(Label, ImVec2(Width, ButtonHeight));
		ImGui::EndDisabled();
		AdvanceToolbar(Width);
	};
	auto DrawMaybeToolbarButton = [&](const char* Label, float Width, bool bEnabled, bool bActive = false)
	{
		if (!bEnabled)
		{
			DrawDisabledToolbarButton(Label, Width);
			return false;
		}
		return DrawToolbarButton(Label, Width, bActive);
	};
	auto DrawMaybeToolbarDropdownButton = [&](const char* Label, const char* Id, float Width, bool bEnabled, bool bActive = false)
	{
		if (!HasToolbarRoom(Width))
		{
			return false;
		}
		if (!bEnabled)
		{
			ImGui::SetCursorScreenPos(ImVec2(CursorX, CursorY));
			ImGui::BeginDisabled();
			DrawToolbarDropdownButton(Label, Id, Width, bActive);
			ImGui::EndDisabled();
			return false;
		}
		return DrawToolbarDropdownButton(Label, Id, Width, bActive);
	};
	auto ModeButton = [&](const char* Label, EPhysicsAssetEditorMode Mode, float Width)
	{
		const bool bActive = ActiveMode == Mode;
		if (DrawToolbarButton(Label, Width, bActive))
		{
			SetEditorMode(Mode);
		}
	};

	constexpr float SaveWidth = 62.0f;
	constexpr float DeleteWidth = 68.0f;
	constexpr float FocusWidth = 62.0f;
	constexpr float BodyModeWidth = 54.0f;
	constexpr float ConstraintModeWidth = 84.0f;
	constexpr float PreviewModeWidth = 64.0f;
	constexpr float SimulateWidth = 86.0f;
	constexpr float CompactModeGroupWidth =
		BodyModeWidth + ConstraintModeWidth + PreviewModeWidth + SimulateWidth + ButtonSpacing * 4.0f;
	constexpr float ModeGroupWidth =
		SaveWidth + DeleteWidth + FocusWidth + BodyModeWidth + ConstraintModeWidth + PreviewModeWidth + SimulateWidth
		+ ButtonSpacing * 8.0f + 10.0f;
	const bool bDrawRightGroup = BarWidth > ModeGroupWidth + 420.0f;
	if (bDrawRightGroup)
	{
		CursorLimitX = BarPos.x + BarWidth - RowPaddingX - ModeGroupWidth - 12.0f;
	}
	else if (BarWidth > CompactModeGroupWidth + 360.0f)
	{
		CursorLimitX = BarPos.x + BarWidth - RowPaddingX - CompactModeGroupWidth - 12.0f;
	}

	bool bShowPreviewMesh = ViewportClient.IsShowPreviewMesh();
	if (DrawToolbarDropdownButton("Preview Mesh", "ToolbarPreviewMesh", 126.0f, bShowPreviewMesh))
	{
		ImGui::OpenPopup("##PhysicsAssetPreviewMeshPopup");
	}
	if (ImGui::BeginPopup("##PhysicsAssetPreviewMeshPopup"))
	{
		ImGui::TextUnformatted("Physics Asset View");
		if (ImGui::MenuItem("Skeletal##ToolbarViewPresetSkeletal", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Skeletal))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Skeletal);
		}
		if (ImGui::MenuItem("Bones##ToolbarViewPresetBones", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Bones))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Bones);
		}
		if (ImGui::MenuItem("Physics##ToolbarViewPresetPhysics", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Physics))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Physics);
		}
		ImGui::Separator();

		bShowPreviewMesh = ViewportClient.IsShowPreviewMesh();
		if (ImGui::Checkbox("Skeletal Mesh##ToolbarShowPreviewMesh", &bShowPreviewMesh))
		{
			ViewportClient.SetShowPreviewMesh(bShowPreviewMesh);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}
		bool bShowSkeleton = ViewportClient.IsShowSkeleton();
		if (ImGui::Checkbox("Bones##ToolbarShowSkeleton", &bShowSkeleton))
		{
			ViewportClient.SetShowSkeleton(bShowSkeleton);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}
		bool bShowBodies = ViewportClient.IsShowBodies();
		if (ImGui::Checkbox("Bodies##ToolbarShowBodies", &bShowBodies))
		{
			ViewportClient.SetShowBodies(bShowBodies);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}
		bool bShowConstraints = ViewportClient.IsShowConstraints();
		if (ImGui::Checkbox("Constraints##ToolbarShowConstraints", &bShowConstraints))
		{
			ViewportClient.SetShowConstraints(bShowConstraints);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}
		ImGui::EndPopup();
	}

	FString CurrentPreviewAnimationPath = "None";
	if (PreviewMeshComponent)
	{
		CurrentPreviewAnimationPath = PreviewMeshComponent->GetAnimationData().AnimToPlayPath.ToString();
	}
	const bool bHasPreviewAnimation = IsValidAssetPath(CurrentPreviewAnimationPath);
	if (DrawMaybeToolbarDropdownButton("Preview Animation", "ToolbarPreviewAnimation", 148.0f, PreviewMeshComponent != nullptr, bHasPreviewAnimation))
	{
		ImGui::OpenPopup("##PhysicsAssetPreviewAnimationPopup");
	}
	if (ImGui::BeginPopup("##PhysicsAssetPreviewAnimationPopup"))
	{
		if (ImGui::Selectable("None", !bHasPreviewAnimation))
		{
			StopPreviewSimulation();
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetAnimationMode(EAnimationMode::None);
				PreviewMeshComponent->ResetBoneEditPose();
				PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
			}
		}

		TArray<FAssetListItem> AnimationItems;
		if (PreviewMesh)
		{
			AnimationItems = FAssetRegistry::ListAnimationsForSkeleton(PreviewMesh->GetSkeletonBinding(), false);
		}
		else
		{
			AnimationItems = FAssetRegistry::ListByTypeName("UAnimSequence");
		}

		if (!AnimationItems.empty())
		{
			ImGui::Separator();
		}
		for (const FAssetListItem& Item : AnimationItems)
		{
			const bool bSelected = CurrentPreviewAnimationPath == Item.FullPath;
			if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
			{
				StopPreviewSimulation();
				UAnimSequence* Sequence = FAnimationManager::Get().LoadAnimation(Item.FullPath);
				if (Sequence && PreviewMeshComponent && PreviewMeshComponent->CanUseAnimation(Sequence))
				{
					PreviewMeshComponent->PlayAnimation(Sequence, true);
					PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
					SetEditorMode(EPhysicsAssetEditorMode::Preview);
				}
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		if (AnimationItems.empty())
		{
			ImGui::TextDisabled("No compatible animations.");
		}
		ImGui::EndPopup();
	}
	DrawSeparator();

	if (DrawToolbarButton("Reference Pose##ToolbarReferencePose", 116.0f) && PreviewMeshComponent)
	{
		StopPreviewSimulation();
		PreviewMeshComponent->ResetBoneEditPose();
	}
	if (DrawMaybeToolbarDropdownButton("Create Asset", "ToolbarCreateAsset", 118.0f, Asset != nullptr))
	{
		ImGui::OpenPopup("##PhysicsAssetCreateAssetPopup");
	}
	if (ImGui::BeginPopup("##PhysicsAssetCreateAssetPopup"))
	{
		if (ImGui::MenuItem("Re-generate Bodies##ToolbarRegenerateBodies"))
		{
			StopPreviewSimulation();
			SetEditorMode(EPhysicsAssetEditorMode::Body);
			RegenerateBodies(Asset);
		}

		const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
		const bool bCanCreateBodyFromBone = Asset
			&& MeshAsset
			&& Selection.Type == EPhysicsAssetEditorSelectionType::Bone
			&& Selection.BoneIndex >= 0
			&& Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size())
			&& Asset->FindBodyIndex(FName(MeshAsset->Bones[Selection.BoneIndex].Name)) < 0;
		if (ImGui::MenuItem("Create Body For Selected Bone##ToolbarCreateBodyFromBone", nullptr, false, bCanCreateBodyFromBone))
		{
			StopPreviewSimulation();
			SetEditorMode(EPhysicsAssetEditorMode::Body);
			CreateBodyForBone(Asset, Selection.BoneIndex);
		}

		const bool bHasBodySelectionForShape = Asset
			&& (Selection.Type == EPhysicsAssetEditorSelectionType::Body || Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
			&& Selection.BodyIndex >= 0
			&& Selection.BodyIndex < static_cast<int32>(Asset->GetBodySetups().size())
			&& Asset->GetBodySetups()[Selection.BodyIndex] != nullptr;
		if (ImGui::BeginMenu("Add Primitive To Selected Body##ToolbarAddPrimitive", bHasBodySelectionForShape))
		{
			UBodySetup* Body = Asset->GetBodySetups()[Selection.BodyIndex];
			if (ImGui::MenuItem("Sphere##ToolbarAddSphere"))
			{
				Body->AddSphere(FVector::ZeroVector, 0.2f);
				SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphere, Body->GetShapeCount(EPhysicsAssetShapeType::Sphere) - 1);
				MarkDirty();
			}
			if (ImGui::MenuItem("Box##ToolbarAddBox"))
			{
				Body->AddBox(FVector::ZeroVector, FQuat::Identity, FVector(0.2f, 0.2f, 0.2f));
				SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Box, Body->GetShapeCount(EPhysicsAssetShapeType::Box) - 1);
				MarkDirty();
			}
			if (ImGui::MenuItem("Capsule##ToolbarAddCapsule"))
			{
				Body->AddSphyl(FVector::ZeroVector, FQuat::Identity, 0.15f, 0.5f);
				SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphyl, Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl) - 1);
				MarkDirty();
			}
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
	DrawSeparator();

	FName SelectedBodyBoneName = FName::None;
	const bool bHasBodySelection = GetSelectedBodyBoneName(Asset, SelectedBodyBoneName);
	const bool bHasConstraintSelection = Asset
		&& Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
		&& Selection.ConstraintIndex >= 0
		&& Selection.ConstraintIndex < static_cast<int32>(Asset->GetConstraintTemplates().size())
		&& Asset->GetConstraintTemplates()[Selection.ConstraintIndex] != nullptr;
	const bool bCanEditCollision = bHasBodySelection || bHasConstraintSelection;
	if (DrawMaybeToolbarButton("Enable Collision##ToolbarEnableCollision", 120.0f, bCanEditCollision))
	{
		if (SetSelectedCollisionEnabled(Asset, true))
		{
			MarkDirty();
		}
	}
	if (DrawMaybeToolbarButton("Disable Collision##ToolbarDisableCollision", 124.0f, bCanEditCollision))
	{
		if (SetSelectedCollisionEnabled(Asset, false))
		{
			MarkDirty();
		}
	}
	static char ToolbarPhysicalMaterialPath[260] = {};
	if (DrawMaybeToolbarDropdownButton("Physical Material", "ToolbarPhysicalMaterial", 136.0f, bHasBodySelection))
	{
		std::snprintf(ToolbarPhysicalMaterialPath, sizeof(ToolbarPhysicalMaterialPath), "%s",
			Asset->GetBodyPhysicalMaterialPath(SelectedBodyBoneName).c_str());
		ImGui::OpenPopup("##PhysicsAssetPhysicalMaterialPopup");
	}
	if (ImGui::BeginPopup("##PhysicsAssetPhysicalMaterialPopup"))
	{
		ImGui::Text("Body: %s", SelectedBodyBoneName.ToString().c_str());
		ImGui::Text("Current: %s", Asset->GetBodyPhysicalMaterialPath(SelectedBodyBoneName).c_str());
		ImGui::Separator();

		const FString CurrentMaterialPath = Asset->GetBodyPhysicalMaterialPath(SelectedBodyBoneName);
		FString SelectedMaterialPath;
		if (DrawPhysicalMaterialPickerOptions(CurrentMaterialPath, SelectedMaterialPath))
		{
			if (SetSelectedPhysicalMaterialPath(Asset, SelectedMaterialPath))
			{
				MarkDirty();
			}
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::BeginMenu("Advanced Object Path"))
		{
			ImGui::SetNextItemWidth(260.0f);
			ImGui::InputText("##ToolbarPhysicalMaterialPath", ToolbarPhysicalMaterialPath, sizeof(ToolbarPhysicalMaterialPath));
			if (ImGui::Button("Use Path", ImVec2(92.0f, 24.0f)))
			{
				if (SetSelectedPhysicalMaterialPath(Asset, FString(ToolbarPhysicalMaterialPath)))
				{
					MarkDirty();
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	DrawSeparator();

	if (DrawMaybeToolbarButton("To Ball & Socket##ToolbarBallSocket", 126.0f, bHasConstraintSelection))
	{
		if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 45.0f, 45.0f, 180.0f))
		{
			MarkDirty();
		}
	}
	if (DrawMaybeToolbarButton("To Hinge##ToolbarHinge", 82.0f, bHasConstraintSelection))
	{
		if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 0.0f, 0.0f, 90.0f))
		{
			MarkDirty();
		}
	}

	if (bDrawRightGroup)
	{
		CursorX = BarPos.x + BarWidth - RowPaddingX - ModeGroupWidth;
		CursorLimitX = BarPos.x + BarWidth - RowPaddingX;
		if (DrawToolbarButton("Save##ToolbarAssetSave", SaveWidth))
		{
			if (FPhysicsAssetManager::Get().Save(Asset))
			{
				ClearDirty();
			}
		}
		if (DrawToolbarButton("Delete##ToolbarAssetDelete", DeleteWidth))
		{
			StopPreviewSimulation();
			if (DeleteSelection(Asset))
			{
				MarkDirty();
			}
		}
		if (DrawToolbarButton("Focus##ToolbarViewportFocus", FocusWidth))
		{
			ViewportClient.ResetCameraToPreviewBounds();
		}
		DrawSeparator();

		ModeButton("Body##ToolbarModeBody", EPhysicsAssetEditorMode::Body, BodyModeWidth);
		ModeButton("Constraint##ToolbarModeConstraint", EPhysicsAssetEditorMode::Constraint, ConstraintModeWidth);
		ModeButton("Preview##ToolbarModePreview", EPhysicsAssetEditorMode::Preview, PreviewModeWidth);

		const bool bSimulatePhysics = ViewportClient.IsSimulatingPhysics();
		if (DrawToolbarButton("Simulate##ToolbarPreviewSimulate", SimulateWidth, bSimulatePhysics))
		{
			if (bSimulatePhysics)
			{
				StopPreviewSimulation();
			}
			else
			{
				ViewportClient.SetSimulatePhysics(true);
			}
		}
	}
	else
	{
		if (BarWidth > CompactModeGroupWidth + 360.0f)
		{
			CursorX = BarPos.x + BarWidth - RowPaddingX - CompactModeGroupWidth;
		}
		CursorLimitX = BarPos.x + BarWidth - RowPaddingX;
		ModeButton("Body##ToolbarModeBody", EPhysicsAssetEditorMode::Body, BodyModeWidth);
		ModeButton("Constraint##ToolbarModeConstraint", EPhysicsAssetEditorMode::Constraint, ConstraintModeWidth);
		ModeButton("Preview##ToolbarModePreview", EPhysicsAssetEditorMode::Preview, PreviewModeWidth);
		const bool bSimulatePhysics = ViewportClient.IsSimulatingPhysics();
		if (DrawToolbarButton("Simulate##ToolbarPreviewSimulate", SimulateWidth, bSimulatePhysics))
		{
			if (bSimulatePhysics)
			{
				StopPreviewSimulation();
			}
			else
			{
				ViewportClient.SetSimulatePhysics(true);
			}
		}
	}

	ImGui::SetCursorScreenPos(ImVec2(BarPos.x, BarPos.y + BarHeight));
}

void FPhysicsAssetEditorWidget::RenderViewportPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("Viewport", Size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	constexpr float ToolbarHeight = 28.0f;
	bool bViewportImageClicked = false;
	if (FViewport* VP = ViewportClient.GetViewport())
	{
		VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), Size);
			FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			bViewportImageClicked = ImGui::IsItemHovered()
				&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				&& MousePos.y > ViewportPos.y + ToolbarHeight;
		}
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	RenderSolidBodiesOverlay(DrawList, ViewportPos, Size, Asset);
	RenderConstraintOverlay(DrawList, ViewportPos, Size, Asset);
	DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight), IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Gizmo = ViewportClient.GetGizmo();
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.bShowGizmoControls = true;
	Context.bShowCameraControls = true;
	Context.bShowViewMode = true;
	Context.bShowShowFlags = true;
	Context.bShowPhysicsAssetShowFlags = true;
	Context.OnCoordSystemToggled = [&]()
	{
		FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
		Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World)
			? EEditorCoordSystem::Local
			: EEditorCoordSystem::World;
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnSettingsChanged = [&]()
	{
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnRenderViewModeExtras = [&]()
	{
		ImGui::TextUnformatted("Physics Asset View");
		if (ImGui::MenuItem("Skeletal##ViewportPreset", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Skeletal))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Skeletal);
		}
		if (ImGui::MenuItem("Bones##ViewportPreset", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Bones))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Bones);
		}
		if (ImGui::MenuItem("Physics##ViewportPreset", nullptr, ActiveViewPreset == EPhysicsAssetEditorViewPreset::Physics))
		{
			ApplyViewPreset(EPhysicsAssetEditorViewPreset::Physics);
		}
		ImGui::Separator();

		bool bShowPreviewMesh = ViewportClient.IsShowPreviewMesh();
		if (ImGui::Checkbox("Skeletal Mesh##ViewportShowPreviewMesh", &bShowPreviewMesh))
		{
			ViewportClient.SetShowPreviewMesh(bShowPreviewMesh);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}

		bool bShowSkeleton = ViewportClient.IsShowSkeleton();
		if (ImGui::Checkbox("Bones##ViewportShowSkeleton", &bShowSkeleton))
		{
			ViewportClient.SetShowSkeleton(bShowSkeleton);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}

		bool bShowBodies = ViewportClient.IsShowBodies();
		if (ImGui::Checkbox("Bodies##ViewportShowBodies", &bShowBodies))
		{
			ViewportClient.SetShowBodies(bShowBodies);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}

		bool bShowConstraints = ViewportClient.IsShowConstraints();
		if (ImGui::Checkbox("Constraints##ViewportShowConstraints", &bShowConstraints))
		{
			ViewportClient.SetShowConstraints(bShowConstraints);
			ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
		}
	};
	FViewportToolbar::Render(Context);
	RenderGizmoForegroundOverlay(DrawList, ViewportPos, Size, ToolbarHeight);

	if (bViewportImageClicked)
	{
		HandleViewportSelectionClick();
	}

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderSolidBodiesOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, UPhysicsAsset* Asset) const
{
	if (!DrawList || !Asset || !PreviewMeshComponent || !ViewportClient.IsShowBodies())
	{
		return;
	}
	if (ViewportSize.x <= 1.0f || ViewportSize.y <= 1.0f)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}
	POV.AspectRatio = ViewportSize.x / ViewportSize.y;

	const FMatrix View = POV.CalculateViewMatrix();
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();

	struct FProjectedVertex
	{
		ImVec2 Screen;
		float Depth = 0.0f;
	};

	struct FProjectedTriangle
	{
		ImVec2 A;
		ImVec2 B;
		ImVec2 C;
		float Depth = 0.0f;
		ImU32 Color = 0;
	};

	std::vector<FProjectedTriangle> Triangles;
	Triangles.reserve(2048);

	auto ProjectWorld = [&](const FVector& World, FProjectedVertex& Out) -> bool
	{
		const FVector ViewPos = View.TransformPositionWithW(World);
		if (ViewPos.Z <= POV.NearClip)
		{
			return false;
		}

		const FVector Clip = ViewProjection.TransformPositionWithW(World);
		if (!std::isfinite(Clip.X) || !std::isfinite(Clip.Y) || !std::isfinite(Clip.Z))
		{
			return false;
		}

		Out.Screen = ImVec2(
			ViewportPos.x + (Clip.X * 0.5f + 0.5f) * ViewportSize.x,
			ViewportPos.y + (1.0f - (Clip.Y * 0.5f + 0.5f)) * ViewportSize.y);
		Out.Depth = ViewPos.Z;
		return true;
	};

	auto AddTriangle = [&](const FVector& A, const FVector& B, const FVector& C, ImU32 Color)
	{
		constexpr size_t MaxSolidOverlayTriangles = 24000;
		if (Triangles.size() >= MaxSolidOverlayTriangles)
		{
			return;
		}

		FProjectedVertex PA, PB, PC;
		if (!ProjectWorld(A, PA) || !ProjectWorld(B, PB) || !ProjectWorld(C, PC))
		{
			return;
		}

		FProjectedTriangle Triangle;
		Triangle.A = PA.Screen;
		Triangle.B = PB.Screen;
		Triangle.C = PC.Screen;
		Triangle.Depth = (PA.Depth + PB.Depth + PC.Depth) / 3.0f;
		Triangle.Color = Color;
		Triangles.push_back(Triangle);
	};

	auto IsSelectedShape = [&](int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex) -> bool
	{
		if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
		{
			return Selection.BodyIndex == BodyIndex;
		}
		if (Selection.Type != EPhysicsAssetEditorSelectionType::Shape)
		{
			return false;
		}
		return Selection.BodyIndex == BodyIndex
			&& Selection.ShapeType == ShapeType
			&& Selection.ShapeIndex == ShapeIndex;
	};

	auto ShapeColor = [&](int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex) -> ImU32
	{
		const TArray<UBodySetup*>& ShapeBodies = Asset->GetBodySetups();
		if (BodyIndex >= 0
			&& BodyIndex < static_cast<int32>(ShapeBodies.size())
			&& ShapeBodies[BodyIndex]
			&& !Asset->IsBodyCollisionEnabled(ShapeBodies[BodyIndex]->GetBoneName()))
		{
			return IsSelectedShape(BodyIndex, ShapeType, ShapeIndex)
				? IM_COL32(255, 92, 32, 96)
				: IM_COL32(130, 130, 130, 30);
		}
		return IsSelectedShape(BodyIndex, ShapeType, ShapeIndex)
			? IM_COL32(255, 118, 0, 122)
			: IM_COL32(0, 162, 255, 48);
	};

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	const int32 BodyCount = static_cast<int32>(Bodies.size());
	const bool bLargeAsset = BodyCount > 48;
	const int32 SphereSlices = bLargeAsset ? 8 : 16;
	const int32 SphereStacks = bLargeAsset ? 4 : 8;
	const int32 CapsuleSlices = bLargeAsset ? 8 : 16;
	const int32 CapsuleHemisphereStacks = bLargeAsset ? 2 : 4;

	auto SpherePoint = [](float Radius, float Theta, float Phi) -> FVector
	{
		const float SinTheta = std::sin(Theta);
		return FVector(
			Radius * SinTheta * std::cos(Phi),
			Radius * SinTheta * std::sin(Phi),
			Radius * std::cos(Theta));
	};

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
	{
		const UBodySetup* Body = Bodies[BodyIndex];
		if (!Body)
		{
			continue;
		}

		FMatrix BoneMatrix;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), BoneMatrix))
		{
			continue;
		}

		const FKAggregateGeom& Geom = Body->GetAggGeom();

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphereElems.size()); ++ShapeIndex)
		{
			const FKSphereElem& Sphere = Geom.SphereElems[ShapeIndex];
			if (Sphere.Radius <= 0.0f)
			{
				continue;
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Sphere, ShapeIndex);
			for (int32 Stack = 0; Stack < SphereStacks; ++Stack)
			{
				const float Theta0 = FMath::Pi * static_cast<float>(Stack) / static_cast<float>(SphereStacks);
				const float Theta1 = FMath::Pi * static_cast<float>(Stack + 1) / static_cast<float>(SphereStacks);
				for (int32 Slice = 0; Slice < SphereSlices; ++Slice)
				{
					const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(SphereSlices);
					const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(SphereSlices);

					const FVector P00 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta0, Phi0));
					const FVector P01 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta0, Phi1));
					const FVector P10 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta1, Phi0));
					const FVector P11 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta1, Phi1));

					AddTriangle(P00, P10, P11, Color);
					AddTriangle(P00, P11, P01, Color);
				}
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.BoxElems.size()); ++ShapeIndex)
		{
			const FKBoxElem& Box = Geom.BoxElems[ShapeIndex];
			if (Box.Extents.X <= 0.0f || Box.Extents.Y <= 0.0f || Box.Extents.Z <= 0.0f)
			{
				continue;
			}

			const FVector Signs[8] = {
				FVector(-1.0f, -1.0f, -1.0f), FVector( 1.0f, -1.0f, -1.0f),
				FVector( 1.0f,  1.0f, -1.0f), FVector(-1.0f,  1.0f, -1.0f),
				FVector(-1.0f, -1.0f,  1.0f), FVector( 1.0f, -1.0f,  1.0f),
				FVector( 1.0f,  1.0f,  1.0f), FVector(-1.0f,  1.0f,  1.0f)
			};

			FVector W[8];
			for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				const FVector LocalCorner(
					Signs[CornerIndex].X * Box.Extents.X,
					Signs[CornerIndex].Y * Box.Extents.Y,
					Signs[CornerIndex].Z * Box.Extents.Z);
				W[CornerIndex] = BoneLocalToWorld(BoneMatrix, ShapeLocalToBoneLocal(Box.Center, Box.Rotation, LocalCorner));
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Box, ShapeIndex);
			const int32 FaceTris[12][3] = {
				{0, 1, 2}, {0, 2, 3},
				{4, 6, 5}, {4, 7, 6},
				{0, 4, 5}, {0, 5, 1},
				{1, 5, 6}, {1, 6, 2},
				{2, 6, 7}, {2, 7, 3},
				{3, 7, 4}, {3, 4, 0}
			};
			for (const int32* Tri : FaceTris)
			{
				AddTriangle(W[Tri[0]], W[Tri[1]], W[Tri[2]], Color);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphylElems.size()); ++ShapeIndex)
		{
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			if (Capsule.Radius <= 0.0f || Capsule.Length <= 0.0f)
			{
				continue;
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Sphyl, ShapeIndex);
			const float HalfLength = Capsule.Length * 0.5f;

			auto CapsulePoint = [&](float Radius, float Z, float Phi) -> FVector
			{
				const FVector ShapeLocal(Radius * std::cos(Phi), Radius * std::sin(Phi), Z);
				return BoneLocalToWorld(BoneMatrix, ShapeLocalToBoneLocal(Capsule.Center, Capsule.Rotation, ShapeLocal));
			};

			for (int32 Slice = 0; Slice < CapsuleSlices; ++Slice)
			{
				const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(CapsuleSlices);
				const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(CapsuleSlices);

				const FVector B0 = CapsulePoint(Capsule.Radius, -HalfLength, Phi0);
				const FVector B1 = CapsulePoint(Capsule.Radius, -HalfLength, Phi1);
				const FVector T0 = CapsulePoint(Capsule.Radius,  HalfLength, Phi0);
				const FVector T1 = CapsulePoint(Capsule.Radius,  HalfLength, Phi1);
				AddTriangle(B0, T0, T1, Color);
				AddTriangle(B0, T1, B1, Color);
			}

			for (int32 Stack = 0; Stack < CapsuleHemisphereStacks; ++Stack)
			{
				const float Theta0 = (FMath::Pi * 0.5f) * static_cast<float>(Stack) / static_cast<float>(CapsuleHemisphereStacks);
				const float Theta1 = (FMath::Pi * 0.5f) * static_cast<float>(Stack + 1) / static_cast<float>(CapsuleHemisphereStacks);
				const float TopR0 = Capsule.Radius * std::cos(Theta0);
				const float TopR1 = Capsule.Radius * std::cos(Theta1);
				const float TopZ0 = HalfLength + Capsule.Radius * std::sin(Theta0);
				const float TopZ1 = HalfLength + Capsule.Radius * std::sin(Theta1);
				const float BotR0 = Capsule.Radius * std::cos(Theta0);
				const float BotR1 = Capsule.Radius * std::cos(Theta1);
				const float BotZ0 = -HalfLength - Capsule.Radius * std::sin(Theta0);
				const float BotZ1 = -HalfLength - Capsule.Radius * std::sin(Theta1);

				for (int32 Slice = 0; Slice < CapsuleSlices; ++Slice)
				{
					const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(CapsuleSlices);
					const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(CapsuleSlices);

					const FVector Top00 = CapsulePoint(TopR0, TopZ0, Phi0);
					const FVector Top01 = CapsulePoint(TopR0, TopZ0, Phi1);
					const FVector Top10 = CapsulePoint(TopR1, TopZ1, Phi0);
					const FVector Top11 = CapsulePoint(TopR1, TopZ1, Phi1);
					AddTriangle(Top00, Top10, Top11, Color);
					AddTriangle(Top00, Top11, Top01, Color);

					const FVector Bot00 = CapsulePoint(BotR0, BotZ0, Phi0);
					const FVector Bot01 = CapsulePoint(BotR0, BotZ0, Phi1);
					const FVector Bot10 = CapsulePoint(BotR1, BotZ1, Phi0);
					const FVector Bot11 = CapsulePoint(BotR1, BotZ1, Phi1);
					AddTriangle(Bot00, Bot11, Bot10, Color);
					AddTriangle(Bot00, Bot01, Bot11, Color);
				}
			}
		}
	}

	std::sort(Triangles.begin(), Triangles.end(),
		[](const FProjectedTriangle& A, const FProjectedTriangle& B)
		{
			return A.Depth > B.Depth;
		});

	DrawList->PushClipRect(ViewportPos, ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y), true);
	const ImDrawListFlags OldDrawListFlags = DrawList->Flags;
	DrawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	for (const FProjectedTriangle& Triangle : Triangles)
	{
		DrawList->AddTriangleFilled(Triangle.A, Triangle.B, Triangle.C, Triangle.Color);
	}
	DrawList->Flags = OldDrawListFlags;
	DrawList->PopClipRect();
}

void FPhysicsAssetEditorWidget::RenderConstraintOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, UPhysicsAsset* Asset) const
{
	if (!DrawList || !Asset || !PreviewMeshComponent || !ViewportClient.IsShowConstraints())
	{
		return;
	}
	if (ViewportSize.x <= 1.0f || ViewportSize.y <= 1.0f)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}
	POV.AspectRatio = ViewportSize.x / ViewportSize.y;

	const FMatrix View = POV.CalculateViewMatrix();
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();

	auto ProjectWorld = [&](const FVector& World, ImVec2& OutScreen) -> bool
	{
		const FVector ViewPos = View.TransformPositionWithW(World);
		if (ViewPos.Z <= POV.NearClip)
		{
			return false;
		}

		const FVector Clip = ViewProjection.TransformPositionWithW(World);
		if (!std::isfinite(Clip.X) || !std::isfinite(Clip.Y) || !std::isfinite(Clip.Z))
		{
			return false;
		}

		OutScreen = ImVec2(
			ViewportPos.x + (Clip.X * 0.5f + 0.5f) * ViewportSize.x,
			ViewportPos.y + (1.0f - (Clip.Y * 0.5f + 0.5f)) * ViewportSize.y);
		return true;
	};

	auto DrawWorldLine = [&](const FVector& A, const FVector& B, ImU32 Color, float Thickness, bool bHalo)
	{
		ImVec2 PA;
		ImVec2 PB;
		if (!ProjectWorld(A, PA) || !ProjectWorld(B, PB))
		{
			return;
		}
		if (bHalo)
		{
			DrawList->AddLine(PA, PB, IM_COL32(0, 0, 0, 150), Thickness + 2.0f);
		}
		DrawList->AddLine(PA, PB, Color, Thickness);
	};

	auto DrawWorldConvexPolygon = [&](const std::vector<FVector>& WorldPoints, ImU32 Color)
	{
		std::vector<ImVec2> ScreenPoints;
		ScreenPoints.reserve(WorldPoints.size());
		for (const FVector& WorldPoint : WorldPoints)
		{
			ImVec2 ScreenPoint;
			if (!ProjectWorld(WorldPoint, ScreenPoint))
			{
				return;
			}
			ScreenPoints.push_back(ScreenPoint);
		}
		if (ScreenPoints.size() < 3)
		{
			return;
		}
		DrawList->AddConvexPolyFilled(ScreenPoints.data(), static_cast<int32>(ScreenPoints.size()), Color);
	};

	auto DrawWorldConvexHull = [&](const std::vector<FVector>& WorldPoints, ImU32 Color)
	{
		std::vector<ImVec2> Points;
		Points.reserve(WorldPoints.size());
		for (const FVector& WorldPoint : WorldPoints)
		{
			ImVec2 ScreenPoint;
			if (!ProjectWorld(WorldPoint, ScreenPoint))
			{
				return;
			}
			Points.push_back(ScreenPoint);
		}
		if (Points.size() < 3)
		{
			return;
		}

		std::sort(Points.begin(), Points.end(), [](const ImVec2& A, const ImVec2& B)
		{
			return A.x == B.x ? A.y < B.y : A.x < B.x;
		});

		auto Cross2D = [](const ImVec2& O, const ImVec2& A, const ImVec2& B) -> float
		{
			return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
		};

		std::vector<ImVec2> Hull;
		Hull.reserve(Points.size() * 2);
		for (const ImVec2& Point : Points)
		{
			while (Hull.size() >= 2 && Cross2D(Hull[Hull.size() - 2], Hull[Hull.size() - 1], Point) <= 0.01f)
			{
				Hull.pop_back();
			}
			Hull.push_back(Point);
		}
		const size_t LowerCount = Hull.size();
		for (int32 Index = static_cast<int32>(Points.size()) - 2; Index >= 0; --Index)
		{
			const ImVec2& Point = Points[Index];
			while (Hull.size() > LowerCount && Cross2D(Hull[Hull.size() - 2], Hull[Hull.size() - 1], Point) <= 0.01f)
			{
				Hull.pop_back();
			}
			Hull.push_back(Point);
		}
		if (!Hull.empty())
		{
			Hull.pop_back();
		}
		if (Hull.size() >= 3)
		{
			DrawList->AddConvexPolyFilled(Hull.data(), static_cast<int32>(Hull.size()), Color);
		}
	};

	auto DrawWorldCircle = [&](const FVector& Center, const FVector& AxisA, const FVector& AxisB, float RadiusA, float RadiusB, ImU32 Color, float Thickness, bool bHalo)
	{
		constexpr int32 Segments = 40;
		FVector PrevPoint = Center + AxisA * RadiusA;
		for (int32 Segment = 1; Segment <= Segments; ++Segment)
		{
			const float T = static_cast<float>(Segment) / static_cast<float>(Segments);
			const float Angle = T * 2.0f * FMath::Pi;
			const FVector Point = Center
				+ AxisA * (std::cos(Angle) * RadiusA)
				+ AxisB * (std::sin(Angle) * RadiusB);
			DrawWorldLine(PrevPoint, Point, Color, Thickness, bHalo);
			PrevPoint = Point;
		}
	};

	auto DrawWorldAxis = [&](const FVector& Origin, const FVector& Axis, const FVector& SideA, const FVector& SideB, float Length, ImU32 Color, float Thickness, bool bHalo)
	{
		const FVector End = Origin + Axis * Length;
		const float HeadLength = Length * 0.22f;
		const float HeadWidth = Length * 0.08f;
		DrawWorldLine(Origin, End, Color, Thickness, bHalo);
		DrawWorldLine(End, End - Axis * HeadLength + SideA * HeadWidth, Color, Thickness, bHalo);
		DrawWorldLine(End, End - Axis * HeadLength - SideA * HeadWidth, Color, Thickness, bHalo);
		DrawWorldLine(End, End - Axis * HeadLength + SideB * HeadWidth, Color, Thickness, bHalo);
		DrawWorldLine(End, End - Axis * HeadLength - SideB * HeadWidth, Color, Thickness, bHalo);
	};

	auto DrawWorldFilledArcFan = [&](const FVector& Center, const FVector& AxisForward, const FVector& AxisSide, float Length, float HalfAngleRad, ImU32 Color)
	{
		const float ClampedHalfAngle = (std::min)(HalfAngleRad, 1.35f);
		constexpr int32 Segments = 24;
		std::vector<FVector> Points;
		Points.reserve(Segments + 2);
		Points.push_back(Center);
		for (int32 Segment = 0; Segment <= Segments; ++Segment)
		{
			const float T = static_cast<float>(Segment) / static_cast<float>(Segments);
			const float Angle = -ClampedHalfAngle + T * ClampedHalfAngle * 2.0f;
			const FVector Point = Center
				+ AxisForward * (std::cos(Angle) * Length)
				+ AxisSide * (std::sin(Angle) * Length);
			Points.push_back(Point);
		}
		DrawWorldConvexPolygon(Points, Color);
	};

	auto DrawWorldFilledDisc = [&](const FVector& Center, const FVector& AxisA, const FVector& AxisB, float RadiusA, float RadiusB, ImU32 Color)
	{
		constexpr int32 Segments = 32;
		std::vector<FVector> Points;
		Points.reserve(Segments);
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const float T = static_cast<float>(Segment) / static_cast<float>(Segments);
			const float Angle = T * 2.0f * FMath::Pi;
			const FVector Point = Center
				+ AxisA * (std::cos(Angle) * RadiusA)
				+ AxisB * (std::sin(Angle) * RadiusB);
			Points.push_back(Point);
		}
		DrawWorldConvexPolygon(Points, Color);
	};

	DrawList->PushClipRect(ViewportPos, ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y), true);

	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
	{
		const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		FMatrix ParentMat;
		FMatrix ChildMat;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Constraint->GetParentBoneName().ToString(), ParentMat)
			|| !PreviewMeshComponent->GetBoneWorldMatrixByName(Constraint->GetChildBoneName().ToString(), ChildMat))
		{
			continue;
		}

		const FVector JointWorld = ParentMat.TransformPositionWithW(Constraint->GetLocalFrameA().Location);
		const FQuat JointRot = ParentMat.ToQuat() * Constraint->GetLocalFrameA().Rotation;
		const FVector DirX = JointRot.RotateVector(FVector::XAxisVector).Normalized();
		const FVector DirY = JointRot.RotateVector(FVector::YAxisVector).Normalized();
		const FVector DirZ = JointRot.RotateVector(FVector::ZAxisVector).Normalized();

		const FVector ParentOrigin = ParentMat.GetLocation();
		const FVector ChildOrigin = ChildMat.GetLocation();
		const float Dist = (ChildOrigin - ParentOrigin).Length();
		const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
			&& Selection.ConstraintIndex == ConstraintIndex;
		const float VisualScale = bSelected ? 1.38f : 1.0f;
		const float ConeLength = Clamp(Dist * 0.10f * VisualScale, 0.03f * VisualScale, 0.13f * VisualScale);
		const ImU32 LimitFillColor = bSelected
			? IM_COL32(172, 255, 56, 150)
			: IM_COL32(172, 255, 56, 95);
		const ImU32 TwistFillColor = bSelected
			? IM_COL32(255, 70, 50, 165)
			: IM_COL32(255, 80, 56, 105);
		const ImU32 LockedFillColor = bSelected
			? IM_COL32(255, 202, 48, 160)
			: IM_COL32(255, 202, 48, 95);
		const ImU32 FreeFillColor = bSelected
			? IM_COL32(110, 190, 255, 125)
			: IM_COL32(110, 190, 255, 70);
		const bool bCollisionDisabled = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
		const ImU32 LinkColor = bSelected
			? IM_COL32(255, 210, 60, 255)
			: (bCollisionDisabled ? IM_COL32(255, 85, 60, 190) : IM_COL32(235, 210, 75, 180));
		const ImU32 LimitOutlineColor = bSelected
			? IM_COL32(255, 238, 80, 255)
			: IM_COL32(255, 226, 75, 210);
		const float GuideThickness = bSelected ? 2.25f : 1.35f;

		DrawWorldLine(ParentOrigin, JointWorld, LinkColor, GuideThickness, bSelected);
		DrawWorldLine(JointWorld, ChildOrigin, LinkColor, GuideThickness, bSelected);

		const EAngularConstraintMode Mode = Constraint->GetAngularMode();
		if (Mode == EAngularConstraintMode::Limited)
		{
			const float Swing1Rad = Constraint->GetSwing1Limit() * (FMath::Pi / 180.0f);
			const float Swing2Rad = Constraint->GetSwing2Limit() * (FMath::Pi / 180.0f);
			const float ClampedSwing1 = (std::min)(Swing1Rad, 1.35f);
			const float ClampedSwing2 = (std::min)(Swing2Rad, 1.35f);
			const float RadiusY = (std::min)(ConeLength * std::tan(ClampedSwing2), ConeLength * 1.05f);
			const float RadiusZ = (std::min)(ConeLength * std::tan(ClampedSwing1), ConeLength * 1.05f);
			const FVector ConeCenter = JointWorld + DirX * ConeLength;

			constexpr int32 ConeSegments = 32;
			std::vector<FVector> ConeSurfacePoints;
			ConeSurfacePoints.reserve(ConeSegments + 1);
			ConeSurfacePoints.push_back(JointWorld);
			for (int32 Segment = 0; Segment < ConeSegments; ++Segment)
			{
				const float T = static_cast<float>(Segment) / static_cast<float>(ConeSegments);
				const float Angle = T * 2.0f * FMath::Pi;
				const FVector Edge = ConeCenter
					+ DirY * (std::cos(Angle) * RadiusY)
					+ DirZ * (std::sin(Angle) * RadiusZ);
				ConeSurfacePoints.push_back(Edge);
			}
			DrawWorldConvexHull(ConeSurfacePoints, LimitFillColor);

			DrawWorldCircle(ConeCenter, DirY, DirZ, RadiusY, RadiusZ, LimitOutlineColor, GuideThickness, bSelected);

			const float TwistRad = Constraint->GetTwistLimit() * (FMath::Pi / 180.0f);
			const float TwistRadius = ConeLength;
			const FVector TwistCenter = JointWorld + DirX * (ConeLength * 0.10f);
			DrawWorldFilledArcFan(TwistCenter, DirY, DirZ, TwistRadius, TwistRad, TwistFillColor);
		}
		else if (Mode == EAngularConstraintMode::Locked)
		{
			const float Radius = ConeLength * 0.24f;
			DrawWorldFilledDisc(JointWorld, DirY, DirZ, Radius, Radius, LockedFillColor);
			DrawWorldCircle(JointWorld, DirY, DirZ, Radius, Radius, LimitOutlineColor, GuideThickness, bSelected);
			DrawWorldLine(JointWorld - DirY * Radius, JointWorld + DirY * Radius, LimitOutlineColor, GuideThickness, bSelected);
			DrawWorldLine(JointWorld - DirZ * Radius, JointWorld + DirZ * Radius, LimitOutlineColor, GuideThickness, bSelected);
		}
		else
		{
			const float Radius = ConeLength * 0.5f;
			DrawWorldFilledDisc(JointWorld, DirY, DirZ, Radius, Radius, FreeFillColor);
			DrawWorldFilledDisc(JointWorld, DirX, DirZ, Radius, Radius, FreeFillColor);
			DrawWorldFilledDisc(JointWorld, DirX, DirY, Radius, Radius, FreeFillColor);
			DrawWorldCircle(JointWorld, DirY, DirZ, Radius, Radius, LimitOutlineColor, GuideThickness, bSelected);
			DrawWorldCircle(JointWorld, DirX, DirZ, Radius, Radius, LimitOutlineColor, GuideThickness, bSelected);
			DrawWorldCircle(JointWorld, DirX, DirY, Radius, Radius, LimitOutlineColor, GuideThickness, bSelected);
		}

		const float AxisLength = ConeLength * (bSelected ? 1.35f : 0.92f);
		const float AxisThickness = bSelected ? 2.1f : 1.25f;
		DrawWorldAxis(JointWorld, DirX, DirY, DirZ, AxisLength, bSelected ? IM_COL32(255, 80, 70, 255) : IM_COL32(255, 80, 70, 190), AxisThickness, bSelected);
		DrawWorldAxis(JointWorld, DirY, DirX, DirZ, AxisLength, bSelected ? IM_COL32(84, 255, 64, 255) : IM_COL32(84, 255, 64, 190), AxisThickness, bSelected);
		DrawWorldAxis(JointWorld, DirZ, DirX, DirY, AxisLength, bSelected ? IM_COL32(78, 150, 255, 245) : IM_COL32(78, 150, 255, 175), AxisThickness, bSelected);

		if (bSelected)
		{
			const FVector ChildFrameWorld = ChildMat.TransformPositionWithW(Constraint->GetLocalFrameB().Location);
			const FQuat ChildFrameRot = ChildMat.ToQuat() * Constraint->GetLocalFrameB().Rotation;
			const FVector ChildDirX = ChildFrameRot.RotateVector(FVector::XAxisVector).Normalized();
			const FVector ChildDirY = ChildFrameRot.RotateVector(FVector::YAxisVector).Normalized();
			const FVector ChildDirZ = ChildFrameRot.RotateVector(FVector::ZAxisVector).Normalized();
			const float ChildAxisLength = AxisLength * 0.65f;
			DrawWorldLine(JointWorld, ChildFrameWorld, IM_COL32(255, 255, 255, 120), 1.0f, false);
			DrawWorldAxis(ChildFrameWorld, ChildDirX, ChildDirY, ChildDirZ, ChildAxisLength, IM_COL32(255, 110, 92, 190), 1.3f, false);
			DrawWorldAxis(ChildFrameWorld, ChildDirY, ChildDirX, ChildDirZ, ChildAxisLength, IM_COL32(112, 255, 92, 190), 1.3f, false);
			DrawWorldAxis(ChildFrameWorld, ChildDirZ, ChildDirX, ChildDirY, ChildAxisLength, IM_COL32(118, 176, 255, 180), 1.3f, false);
		}
	}

	DrawList->PopClipRect();
}

void FPhysicsAssetEditorWidget::RenderGizmoForegroundOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, float ToolbarHeight) const
{
	UGizmoComponent* Gizmo = ViewportClient.GetGizmo();
	if (!DrawList || !Gizmo || !Gizmo->HasTarget() || ViewportClient.IsSimulatingPhysics())
	{
		return;
	}
	if (ViewportSize.x <= 1.0f || ViewportSize.y <= 1.0f)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}
	POV.AspectRatio = ViewportSize.x / ViewportSize.y;

	const FMatrix View = POV.CalculateViewMatrix();
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();

	auto ProjectWorld = [&](const FVector& World, ImVec2& OutScreen) -> bool
	{
		const FVector ViewPos = View.TransformPositionWithW(World);
		if (ViewPos.Z <= POV.NearClip)
		{
			return false;
		}

		const FVector Clip = ViewProjection.TransformPositionWithW(World);
		if (!std::isfinite(Clip.X) || !std::isfinite(Clip.Y) || !std::isfinite(Clip.Z))
		{
			return false;
		}

		OutScreen = ImVec2(
			ViewportPos.x + (Clip.X * 0.5f + 0.5f) * ViewportSize.x,
			ViewportPos.y + (1.0f - (Clip.Y * 0.5f + 0.5f)) * ViewportSize.y);
		return true;
	};

	auto AxisColor = [&](int32 Axis) -> ImU32
	{
		if (Gizmo->GetSelectedAxis() == Axis)
		{
			return IM_COL32(255, 220, 72, 255);
		}
		switch (Axis)
		{
			case 0: return IM_COL32(255, 76, 76, 255);
			case 1: return IM_COL32(72, 230, 72, 255);
			case 2: return IM_COL32(76, 128, 255, 255);
			default: return IM_COL32(230, 230, 230, 255);
		}
	};

	auto DrawScreenLine = [&](const ImVec2& A, const ImVec2& B, ImU32 Color, float Thickness)
	{
		DrawList->AddLine(A, B, Color, Thickness);
	};

	auto DrawWorldLine = [&](const FVector& A, const FVector& B, ImU32 Color, float Thickness)
	{
		ImVec2 PA;
		ImVec2 PB;
		if (!ProjectWorld(A, PA) || !ProjectWorld(B, PB))
		{
			return;
		}
		DrawScreenLine(PA, PB, Color, Thickness);
	};

	auto DrawWorldTriangle = [&](const FVector& A, const FVector& B, const FVector& C, ImU32 Color)
	{
		ImVec2 PA;
		ImVec2 PB;
		ImVec2 PC;
		if (!ProjectWorld(A, PA) || !ProjectWorld(B, PB) || !ProjectWorld(C, PC))
		{
			return;
		}
		DrawList->AddTriangleFilled(PA, PB, PC, Color);
	};

	auto DrawRotateRing = [&](const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, ImU32 Color, float Thickness)
	{
		constexpr int32 Segments = 72;
		FVector PrevPoint;
		bool bHasPrev = false;
		for (int32 Segment = 0; Segment <= Segments; ++Segment)
		{
			const float T = static_cast<float>(Segment) / static_cast<float>(Segments);
			const float Angle = T * 2.0f * FMath::Pi;
			const FVector Point = Center
				+ AxisA * (std::cos(Angle) * Radius)
				+ AxisB * (std::sin(Angle) * Radius);
			if (bHasPrev)
			{
				DrawWorldLine(PrevPoint, Point, Color, Thickness);
			}
			PrevPoint = Point;
			bHasPrev = true;
		}
	};

	auto DrawScaleBox = [&](const ImVec2& End, ImU32 Color)
	{
		const float Half = 5.5f;
		DrawList->AddRectFilled(ImVec2(End.x - Half, End.y - Half), ImVec2(End.x + Half, End.y + Half), Color, 1.5f);
	};

	auto DrawTranslateCone = [&](const FVector& Tip, const FVector& AxisDir, float AxisScale, ImU32 Color, float Thickness)
	{
		const float ConeLength = AxisScale * 0.18f;
		const float ConeRadius = AxisScale * 0.075f;
		const FVector BaseCenter = Tip - AxisDir * ConeLength;
		FVector BasisA = AxisDir.Cross(POV.Rotation.GetForwardVector()).Normalized();
		if (BasisA.Length() <= 1.e-4f)
		{
			BasisA = AxisDir.Cross(FVector::UpVector).Normalized();
		}
		if (BasisA.Length() <= 1.e-4f)
		{
			BasisA = AxisDir.Cross(FVector::RightVector).Normalized();
		}
		const FVector BasisB = AxisDir.Cross(BasisA).Normalized();

		constexpr int32 ConeSegments = 18;
		FVector PrevEdge = BaseCenter + BasisA * ConeRadius;
		for (int32 Segment = 1; Segment <= ConeSegments; ++Segment)
		{
			const float T = static_cast<float>(Segment) / static_cast<float>(ConeSegments);
			const float Angle = T * 2.0f * FMath::Pi;
			const FVector Edge = BaseCenter
				+ BasisA * (std::cos(Angle) * ConeRadius)
				+ BasisB * (std::sin(Angle) * ConeRadius);
			DrawWorldTriangle(Tip, PrevEdge, Edge, Color);
			DrawWorldTriangle(BaseCenter, Edge, PrevEdge, Color);
			PrevEdge = Edge;
		}
	};

	const FVector Center = Gizmo->GetWorldLocation();
	const float Scale = Gizmo->ComputeScreenSpaceScale(POV.Location, POV.bIsOrtho, POV.OrthoWidth);
	const uint32 AxisMask = Gizmo->GetAxisMask();
	const EGizmoMode Mode = Gizmo->GetMode();

	DrawList->PushClipRect(
		ImVec2(ViewportPos.x, ViewportPos.y + ToolbarHeight),
		ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y),
		true);

	ImVec2 CenterScreen;
	if (ProjectWorld(Center, CenterScreen))
	{
		DrawList->AddCircleFilled(CenterScreen, 5.5f, IM_COL32(0, 0, 0, 230), 18);
		DrawList->AddCircleFilled(CenterScreen, 3.3f, IM_COL32(235, 235, 235, 255), 18);
	}

	if (Mode == EGizmoMode::Rotate)
	{
		const FVector AxisX = Gizmo->GetVectorForAxis(0).Normalized();
		const FVector AxisY = Gizmo->GetVectorForAxis(1).Normalized();
		const FVector AxisZ = Gizmo->GetVectorForAxis(2).Normalized();
		const FVector RingAxes[3][2] = {
			{ AxisY, AxisZ },
			{ AxisX, AxisZ },
			{ AxisX, AxisY }
		};
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if ((AxisMask & (1u << Axis)) == 0)
			{
				continue;
			}
			DrawRotateRing(Center, RingAxes[Axis][0], RingAxes[Axis][1], Scale, AxisColor(Axis), Gizmo->GetSelectedAxis() == Axis ? 3.4f : 2.4f);
		}
	}
	else
	{
		const bool bScaleHandle = Mode == EGizmoMode::Scale;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if ((AxisMask & (1u << Axis)) == 0)
			{
				continue;
			}

			const FVector AxisDir = Gizmo->GetVectorForAxis(Axis).Normalized();
			const FVector End = Center + AxisDir * Scale;
			ImVec2 StartScreen;
			ImVec2 EndScreen;
			if (!ProjectWorld(Center, StartScreen) || !ProjectWorld(End, EndScreen))
			{
				continue;
			}

			const ImU32 Color = AxisColor(Axis);
			const float Thickness = Gizmo->GetSelectedAxis() == Axis ? 4.2f : 3.2f;
			if (bScaleHandle)
			{
				DrawScreenLine(StartScreen, EndScreen, Color, Thickness);
				DrawScaleBox(EndScreen, Color);
			}
			else
			{
				const FVector StemEnd = End - AxisDir * (Scale * 0.18f);
				ImVec2 StemEndScreen;
				if (ProjectWorld(StemEnd, StemEndScreen))
				{
					DrawScreenLine(StartScreen, StemEndScreen, Color, Thickness);
				}
				DrawTranslateCone(End, AxisDir, Scale, Color, Thickness);
			}
		}
	}

	DrawList->PopClipRect();
}

void FPhysicsAssetEditorWidget::RenderPhysicsListPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetListPanel", Size, true);
	DrawPhysicsPanelHeader("Physics Bodies / Constraints");

	if (!Asset)
	{
		ImGui::EndChild();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.045f, 0.045f, 0.045f, 1.0f));
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##PhysicsAssetListFilter", "Search Bodies, Shapes, Constraints", ListFilter, sizeof(ListFilter));
	ImGui::PopStyleColor();
	const FString Filter = ListFilter;

	if (ImGui::BeginTabBar("##PhysicsAssetListTabs"))
	{
		if (ImGui::BeginTabItem("Bodies"))
		{
			const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
			for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
			{
				UBodySetup* Body = Bodies[BodyIndex];
				if (!Body)
				{
					continue;
				}

				const FString BodyLabel = Body->GetBoneName().ToString();
				if (!Filter.empty() && BodyLabel.find(Filter) == FString::npos)
				{
					continue;
				}

				ImGui::PushID(BodyIndex);
				const bool bBodySelected = Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex;
				ImGuiTreeNodeFlags BodyFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (bBodySelected)
				{
					BodyFlags |= ImGuiTreeNodeFlags_Selected;
				}

				const bool bOpen = ImGui::TreeNodeEx("Body", BodyFlags, "%s", BodyLabel.c_str());
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					SelectBody(BodyIndex);
				}

				if (bOpen)
				{
					const EPhysicsAssetShapeType ShapeTypes[] = {
						EPhysicsAssetShapeType::Sphere,
						EPhysicsAssetShapeType::Box,
						EPhysicsAssetShapeType::Sphyl,
						EPhysicsAssetShapeType::Convex
					};

					for (EPhysicsAssetShapeType ShapeType : ShapeTypes)
					{
						const int32 ShapeCount = Body->GetShapeCount(ShapeType);
						for (int32 ShapeIndex = 0; ShapeIndex < ShapeCount; ++ShapeIndex)
						{
							ImGui::PushID(static_cast<int32>(ShapeType) * 1000 + ShapeIndex);
							const bool bShapeSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Shape
								&& Selection.BodyIndex == BodyIndex
								&& Selection.ShapeType == ShapeType
								&& Selection.ShapeIndex == ShapeIndex;
							char Label[64];
							std::snprintf(Label, sizeof(Label), "%s %d", ShapeTypeLabel(ShapeType), ShapeIndex);
							if (ImGui::Selectable(Label, bShapeSelected))
							{
								SelectShape(BodyIndex, ShapeType, ShapeIndex);
							}
							ImGui::PopID();
						}
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Constraints"))
		{
			const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
			for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
			{
				const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
				if (!Constraint)
				{
					continue;
				}

				FString Label = Constraint->GetParentBoneName().ToString();
				Label += " -> ";
				Label += Constraint->GetChildBoneName().ToString();
				if (!Filter.empty() && Label.find(Filter) == FString::npos)
				{
					continue;
				}

				ImGui::PushID(ConstraintIndex);
				const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
					&& Selection.ConstraintIndex == ConstraintIndex;
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					SelectConstraint(ConstraintIndex);
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Profiles"))
		{
			const TArray<UBodySetup*>& BodiesForProfiles = Asset->GetBodySetups();
			ImGui::Text("Body Profiles: %llu", static_cast<unsigned long long>(BodiesForProfiles.size()));
			for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodiesForProfiles.size()); ++BodyIndex)
			{
				const UBodySetup* Body = BodiesForProfiles[BodyIndex];
				if (!Body)
				{
					continue;
				}
				const FName BoneName = Body->GetBoneName();
				if (!Filter.empty() && BoneName.ToString().find(Filter) == FString::npos)
				{
					continue;
				}

				ImGui::PushID(BodyIndex);
				if (ImGui::Selectable(BoneName.ToString().c_str(), Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex))
				{
					SelectBody(BodyIndex);
				}
				ImGui::SameLine();
				ImGui::TextDisabled(Asset->IsBodyCollisionEnabled(BoneName) ? "Collision" : "No Collision");
				ImGui::PopID();
			}

			ImGui::Separator();
			const TArray<FPhysicsAssetCollisionDisablePair>& DisabledPairs = Asset->GetDisabledCollisionPairs();
			ImGui::Text("Disabled Pairs: %llu", static_cast<unsigned long long>(DisabledPairs.size()));
			for (int32 PairIndex = 0; PairIndex < static_cast<int32>(DisabledPairs.size()); ++PairIndex)
			{
				const FPhysicsAssetCollisionDisablePair& Pair = DisabledPairs[PairIndex];
				FString Label = Pair.BoneA.ToString();
				Label += " <-> ";
				Label += Pair.BoneB.ToString();
				if (!Filter.empty() && Label.find(Filter) == FString::npos)
				{
					continue;
				}
				ImGui::BulletText("%s", Label.c_str());
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderSkeletonTreePanel(UPhysicsAsset* Asset)
{
	DrawPhysicsPanelHeader("Skeleton Tree");

	if (!Asset)
	{
		return;
	}

	if (DrawSmallToolbarButton("+##SkeletonTreeAdd", ImVec2(34.0f, 24.0f), true))
	{
		if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone)
		{
			CreateBodyForBone(Asset, Selection.BoneIndex);
		}
	}
	ImGui::SameLine();
	const float AvailableSearchWidth = ImGui::GetContentRegionAvail().x - 30.0f;
	const float SearchWidth = AvailableSearchWidth > 80.0f ? AvailableSearchWidth : 80.0f;
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.045f, 0.045f, 0.045f, 1.0f));
	ImGui::SetNextItemWidth(SearchWidth);
	ImGui::InputTextWithHint("##PhysicsAssetTreeFilter", "Search Skeleton Tree...", TreeFilter, sizeof(TreeFilter));
	ImGui::PopStyleColor();
	ImGui::SameLine();
	DrawSmallToolbarButton("...##SkeletonTreeOptions", ImVec2(24.0f, 24.0f));
	const FString Filter = TreeFilter;

	if (PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
	{
		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (ImGui::TreeNodeEx("Skeleton", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
			{
				if (MeshAsset->Bones[BoneIndex].ParentIndex == -1
					&& BoneSubtreeMatchesFilter(MeshAsset, BoneIndex, Filter))
				{
					RenderBoneTreeNode(MeshAsset, Asset, BoneIndex);
				}
			}
			ImGui::TreePop();
		}
	}
}

void FPhysicsAssetEditorWidget::RenderBoneTreeNode(const FSkeletalMesh* MeshAsset, UPhysicsAsset* Asset, int32 BoneIndex)
{
	if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
	{
		return;
	}

	const FBone& Bone = MeshAsset->Bones[BoneIndex];
	const bool bHasBody = Asset && Asset->FindBodyIndex(FName(Bone.Name)) >= 0;
	const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Bone && Selection.BoneIndex == BoneIndex;

	bool bHasChild = false;
	for (const FBone& Candidate : MeshAsset->Bones)
	{
		if (Candidate.ParentIndex == BoneIndex)
		{
			bHasChild = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasChild)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	FString Label = Bone.Name;
	if (bHasBody)
	{
		Label += "  [Body]";
	}

	ImGui::PushID(BoneIndex);
	const bool bOpen = ImGui::TreeNodeEx("Bone", Flags, "%s", Label.c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		SelectBone(BoneIndex);
	}

	if (bHasChild && bOpen)
	{
		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(MeshAsset->Bones.size()); ++ChildIndex)
		{
			if (MeshAsset->Bones[ChildIndex].ParentIndex == BoneIndex
				&& BoneSubtreeMatchesFilter(MeshAsset, ChildIndex, TreeFilter))
			{
				RenderBoneTreeNode(MeshAsset, Asset, ChildIndex);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FPhysicsAssetEditorWidget::RenderGraphPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetGraphPanel", Size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	DrawPhysicsPanelHeader("Physics Asset Graph");

	if (!Asset)
	{
		ImGui::EndChild();
		return;
	}

	EnsureGraphEditorContext();
	if (!GraphEditorContext)
	{
		ImGui::TextDisabled("Graph editor is unavailable.");
		ImGui::EndChild();
		return;
	}

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
	bool bSelectionDrivenGraph = Selection.Type != EPhysicsAssetEditorSelectionType::None && !bSelectionOriginatedFromGraph;

	std::vector<int32> RootBodyIndices;
	std::vector<int32> GraphBodyIndices;
	std::vector<int32> GraphConstraintIndices;

	auto AddUniqueIndex = [](std::vector<int32>& Values, int32 Index)
	{
		if (Index >= 0 && std::find(Values.begin(), Values.end(), Index) == Values.end())
		{
			Values.push_back(Index);
		}
	};

	auto HasIndex = [](const std::vector<int32>& Values, int32 Index) -> bool
	{
		return std::find(Values.begin(), Values.end(), Index) != Values.end();
	};

	auto AddBodyByBone = [&](FName BoneName) -> int32
	{
		const int32 BodyIndex = Asset->FindBodyIndex(BoneName);
		if (BodyIndex >= 0)
		{
			AddUniqueIndex(GraphBodyIndices, BodyIndex);
		}
		return BodyIndex;
	};

	auto AddConstraintAndBodies = [&](int32 ConstraintIndex)
	{
		if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(Constraints.size()) || !Constraints[ConstraintIndex])
		{
			return;
		}

		AddUniqueIndex(GraphConstraintIndices, ConstraintIndex);
		AddBodyByBone(Constraints[ConstraintIndex]->GetParentBoneName());
		AddBodyByBone(Constraints[ConstraintIndex]->GetChildBoneName());
	};

	if (bSelectionDrivenGraph)
	{
		if (Selection.Type == EPhysicsAssetEditorSelectionType::Body || Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
		{
			AddUniqueIndex(RootBodyIndices, Selection.BodyIndex);
			AddUniqueIndex(GraphBodyIndices, Selection.BodyIndex);
		}
		else if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone && MeshAsset
			&& Selection.BoneIndex >= 0 && Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size()))
		{
			const int32 BodyIndex = Asset->FindBodyIndex(FName(MeshAsset->Bones[Selection.BoneIndex].Name));
			AddUniqueIndex(RootBodyIndices, BodyIndex);
			AddUniqueIndex(GraphBodyIndices, BodyIndex);
		}
		else if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
		{
			AddConstraintAndBodies(Selection.ConstraintIndex);
			if (Selection.ConstraintIndex >= 0 && Selection.ConstraintIndex < static_cast<int32>(Constraints.size()) && Constraints[Selection.ConstraintIndex])
			{
				AddUniqueIndex(RootBodyIndices, Asset->FindBodyIndex(Constraints[Selection.ConstraintIndex]->GetParentBoneName()));
			}
		}

		if (!RootBodyIndices.empty())
		{
			const std::vector<int32> InitialRoots = RootBodyIndices;
			for (int32 RootBodyIndex : InitialRoots)
			{
				if (RootBodyIndex < 0 || RootBodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[RootBodyIndex])
				{
					continue;
				}

				const FName RootBoneName = Bodies[RootBodyIndex]->GetBoneName();
				for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
				{
					const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
					if (!Constraint)
					{
						continue;
					}

					if (Constraint->GetParentBoneName() == RootBoneName)
					{
						AddUniqueIndex(GraphConstraintIndices, ConstraintIndex);
						AddBodyByBone(Constraint->GetChildBoneName());
					}
					else if (Constraint->GetChildBoneName() == RootBoneName)
					{
						AddUniqueIndex(GraphConstraintIndices, ConstraintIndex);
						AddBodyByBone(Constraint->GetParentBoneName());
					}
				}
			}
		}
	}

	if (GraphBodyIndices.empty() && GraphConstraintIndices.empty())
	{
		bSelectionDrivenGraph = false;
		for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
		{
			AddUniqueIndex(GraphBodyIndices, BodyIndex);
		}
		for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
		{
			AddUniqueIndex(GraphConstraintIndices, ConstraintIndex);
		}
	}

	if (GraphBodyIndices.empty() && GraphConstraintIndices.empty())
	{
		ImGui::TextDisabled("No physics bodies or constraints.");
		ImGui::EndChild();
		return;
	}

	const float GraphCanvasWidth = (std::max)(ImGui::GetContentRegionAvail().x, 240.0f);
	const float SelectionGraphRootX = 24.0f;
	const float SelectionGraphConstraintX = (std::max)(118.0f, (std::min)(GraphCanvasWidth * 0.42f, 210.0f));
	const float SelectionGraphLinkedX = (std::max)(220.0f, (std::min)(GraphCanvasWidth - 130.0f, 360.0f));

	ed::SetCurrentEditor(GraphEditorContext);
	ed::Begin("PhysicsAssetGraphCanvas");

	bool bNavigateToGraphContent = false;
	if (!bGraphPositionsPushed)
	{
		if (bSelectionDrivenGraph)
		{
			int32 RootRow = 0;
			int32 LinkedRow = 0;
			for (int32 BodyIndex : GraphBodyIndices)
			{
				const bool bIsRoot = HasIndex(RootBodyIndices, BodyIndex);
				ed::SetNodePosition(ToPhysicsGraphNodeId(BodyIndex), ImVec2(
					bIsRoot ? SelectionGraphRootX : SelectionGraphLinkedX,
					40.0f + static_cast<float>(bIsRoot ? RootRow++ : LinkedRow++) * 92.0f));
			}

			for (int32 Index = 0; Index < static_cast<int32>(GraphConstraintIndices.size()); ++Index)
			{
				ed::SetNodePosition(ToPhysicsGraphConstraintNodeId(GraphConstraintIndices[Index]), ImVec2(
					SelectionGraphConstraintX,
					40.0f + static_cast<float>(Index) * 74.0f));
			}
		}
		else
		{
			std::vector<int32> RowsByDepth;
			for (int32 BodyIndex : GraphBodyIndices)
			{
				const UBodySetup* Body = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size())) ? Bodies[BodyIndex] : nullptr;
				if (!Body)
				{
					continue;
				}

				float SavedX = 0.0f;
				float SavedY = 0.0f;
				if (Asset->GetGraphNodePosition(Body->GetBoneName(), SavedX, SavedY))
				{
					ed::SetNodePosition(ToPhysicsGraphNodeId(BodyIndex), ImVec2(SavedX, SavedY));
					continue;
				}

				int32 Depth = 0;
				if (MeshAsset)
				{
					int32 BoneIndex = -1;
					for (int32 Index = 0; Index < static_cast<int32>(MeshAsset->Bones.size()); ++Index)
					{
						if (MeshAsset->Bones[Index].Name == Body->GetBoneName().ToString())
						{
							BoneIndex = Index;
							break;
						}
					}
					for (int32 Walk = BoneIndex; Walk >= 0 && Walk < static_cast<int32>(MeshAsset->Bones.size()); Walk = MeshAsset->Bones[Walk].ParentIndex)
					{
						++Depth;
					}
					Depth = (std::max)(0, Depth - 1);
				}
				else
				{
					Depth = BodyIndex % 2;
				}

				if (Depth >= static_cast<int32>(RowsByDepth.size()))
				{
					RowsByDepth.resize(Depth + 1, 0);
				}

				const int32 Row = RowsByDepth[Depth]++;
				ed::SetNodePosition(ToPhysicsGraphNodeId(BodyIndex), ImVec2(
					40.0f + static_cast<float>(Depth) * 210.0f,
					40.0f + static_cast<float>(Row) * 92.0f));
			}

			for (int32 Index = 0; Index < static_cast<int32>(GraphConstraintIndices.size()); ++Index)
			{
				ed::SetNodePosition(ToPhysicsGraphConstraintNodeId(GraphConstraintIndices[Index]), ImVec2(
					260.0f,
					40.0f + static_cast<float>(Index) * 74.0f));
			}
		}
		bGraphPositionsPushed = true;
		bNavigateToGraphContent = true;
	}

	for (int32 BodyIndex : GraphBodyIndices)
	{
		const UBodySetup* Body = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size())) ? Bodies[BodyIndex] : nullptr;
		if (!Body)
		{
			continue;
		}

		const int32 ShapeCount =
			Body->GetShapeCount(EPhysicsAssetShapeType::Sphere)
			+ Body->GetShapeCount(EPhysicsAssetShapeType::Box)
			+ Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl)
			+ Body->GetShapeCount(EPhysicsAssetShapeType::Convex);
		const bool bCollisionEnabled = Asset->IsBodyCollisionEnabled(Body->GetBoneName());
		const bool bSelectedBody = (Selection.Type == EPhysicsAssetEditorSelectionType::Body || Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
			&& Selection.BodyIndex == BodyIndex;

		ed::BeginNode(ToPhysicsGraphNodeId(BodyIndex));
			ImGui::PushID(BodyIndex);
			ImGui::TextColored(bSelectedBody ? ImVec4(1.0f, 0.73f, 0.25f, 1.0f) : ImVec4(0.55f, 0.82f, 0.42f, 1.0f), "Body");
			ImGui::SameLine();
			ImGui::TextUnformatted(Body->GetBoneName().ToString().c_str());
			if (!bCollisionEnabled)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.95f, 0.46f, 0.34f, 1.0f), "No Collision");
			}
			ImGui::Separator();
			ed::BeginPin(ToPhysicsGraphInputPinId(BodyIndex), ed::PinKind::Input);
				ImGui::TextUnformatted("< Parent");
			ed::EndPin();
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(12.0f, 1.0f));
			ImGui::SameLine();
			ed::BeginPin(ToPhysicsGraphOutputPinId(BodyIndex), ed::PinKind::Output);
				ImGui::TextUnformatted("Child >");
			ed::EndPin();
			ImGui::TextDisabled("%d shape(s)", ShapeCount);
			ImGui::PopID();
		ed::EndNode();
	}

	for (int32 ConstraintIndex : GraphConstraintIndices)
	{
		const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		const bool bCollisionDisabled = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
		const bool bSelectedConstraint = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
			&& Selection.ConstraintIndex == ConstraintIndex;

		ed::BeginNode(ToPhysicsGraphConstraintNodeId(ConstraintIndex));
			ImGui::PushID(PhysicsGraphConstraintNodeBase + static_cast<uint32>(ConstraintIndex));
			ImGui::TextColored(bSelectedConstraint ? ImVec4(1.0f, 0.73f, 0.25f, 1.0f) : ImVec4(0.84f, 0.78f, 0.46f, 1.0f), "Constraint");
			ImGui::Separator();
			ed::BeginPin(ToPhysicsGraphConstraintInputPinId(ConstraintIndex), ed::PinKind::Input);
				ImGui::TextUnformatted("< Joint");
			ed::EndPin();
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(14.0f, 1.0f));
			ImGui::SameLine();
			ed::BeginPin(ToPhysicsGraphConstraintOutputPinId(ConstraintIndex), ed::PinKind::Output);
				ImGui::TextUnformatted("Body >");
			ed::EndPin();
			ImGui::Text("%s", AngularModeLabel(Constraint->GetAngularMode()));
			if (bCollisionDisabled)
			{
				ImGui::TextColored(ImVec4(0.95f, 0.46f, 0.34f, 1.0f), "Collision Disabled");
			}
			ImGui::PopID();
		ed::EndNode();
	}

	for (int32 ConstraintIndex : GraphConstraintIndices)
	{
		const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		const int32 ParentIndex = Asset->FindBodyIndex(Constraint->GetParentBoneName());
		const int32 ChildIndex = Asset->FindBodyIndex(Constraint->GetChildBoneName());
		if (ParentIndex < 0 || ChildIndex < 0 || !HasIndex(GraphBodyIndices, ParentIndex) || !HasIndex(GraphBodyIndices, ChildIndex))
		{
			continue;
		}

		int32 SourceBodyIndex = ParentIndex;
		int32 TargetBodyIndex = ChildIndex;
		if (bSelectionDrivenGraph && HasIndex(RootBodyIndices, ChildIndex) && !HasIndex(RootBodyIndices, ParentIndex))
		{
			SourceBodyIndex = ChildIndex;
			TargetBodyIndex = ParentIndex;
		}

		const ImVec4 LinkColor = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName())
			? ImVec4(0.95f, 0.38f, 0.30f, 1.0f)
			: ImVec4(0.85f, 0.78f, 0.44f, 1.0f);
		ed::Link(ToPhysicsGraphLinkId(ConstraintIndex),
			ToPhysicsGraphOutputPinId(SourceBodyIndex),
			ToPhysicsGraphConstraintInputPinId(ConstraintIndex),
			LinkColor,
			Selection.Type == EPhysicsAssetEditorSelectionType::Constraint && Selection.ConstraintIndex == ConstraintIndex ? 3.0f : 1.6f);
		ed::Link(ToPhysicsGraphLinkedBodyLinkId(ConstraintIndex),
			ToPhysicsGraphConstraintOutputPinId(ConstraintIndex),
			ToPhysicsGraphInputPinId(TargetBodyIndex),
			LinkColor,
			Selection.Type == EPhysicsAssetEditorSelectionType::Constraint && Selection.ConstraintIndex == ConstraintIndex ? 3.0f : 1.6f);
	}

	if (bPendingGraphSelectionSync)
	{
		SyncGraphEditorSelectionToSelection();
	}

	if (bNavigateToGraphContent)
	{
		ed::NavigateToContent(0.0f);
	}

	if (ed::BeginCreate())
	{
		ed::PinId StartId;
		ed::PinId EndId;
		if (ed::QueryNewLink(&StartId, &EndId))
		{
			bool bStartOutput = false;
			bool bEndOutput = false;
			const int32 StartBodyIndex = PhysicsGraphPinIdToBodyIndex(StartId, bStartOutput);
			const int32 EndBodyIndex = PhysicsGraphPinIdToBodyIndex(EndId, bEndOutput);
			const bool bValidLink = StartId && EndId
				&& StartBodyIndex >= 0
				&& EndBodyIndex >= 0
				&& StartBodyIndex != EndBodyIndex
				&& bStartOutput != bEndOutput;

			if (bValidLink)
			{
				const int32 ParentBodyIndex = bStartOutput ? StartBodyIndex : EndBodyIndex;
				const int32 ChildBodyIndex = bStartOutput ? EndBodyIndex : StartBodyIndex;
				const UBodySetup* ParentBody = Bodies[ParentBodyIndex];
				const UBodySetup* ChildBody = Bodies[ChildBodyIndex];
				const bool bNoExistingConstraint = ParentBody
					&& ChildBody
					&& Asset->FindConstraintIndex(ParentBody->GetBoneName(), ChildBody->GetBoneName()) < 0
					&& Asset->FindConstraintIndex(ChildBody->GetBoneName(), ParentBody->GetBoneName()) < 0;

				if (bNoExistingConstraint)
				{
					if (ed::AcceptNewItem())
					{
						UPhysicsConstraintTemplate* NewConstraint = CreateConstraintBetweenBodies(Asset, ParentBodyIndex, ChildBodyIndex);
						if (NewConstraint)
						{
							SelectConstraint(static_cast<int32>(Asset->GetConstraintTemplates().size()) - 1, true);
							bGraphPositionsPushed = false;
							MarkDirty();
						}
					}
				}
				else
				{
					ed::RejectNewItem(ImVec4(1.0f, 0.30f, 0.30f, 1.0f), 2.0f);
				}
			}
			else
			{
				ed::RejectNewItem(ImVec4(1.0f, 0.30f, 0.30f, 1.0f), 2.0f);
			}
		}
	}
	ed::EndCreate();

	std::vector<int32> ConstraintDeletes;
	std::vector<int32> BodyDeletes;
	if (ed::BeginDelete())
	{
		ed::LinkId DeletedLink;
		while (ed::QueryDeletedLink(&DeletedLink))
		{
			if (ed::AcceptDeletedItem())
			{
				ConstraintDeletes.push_back(PhysicsGraphLinkIdToConstraintIndex(DeletedLink));
			}
		}

		ed::NodeId DeletedNode;
		while (ed::QueryDeletedNode(&DeletedNode))
		{
			if (ed::AcceptDeletedItem())
			{
				const int32 ConstraintIndex = PhysicsGraphNodeIdToConstraintIndex(DeletedNode);
				if (ConstraintIndex >= 0)
				{
					ConstraintDeletes.push_back(ConstraintIndex);
				}
				else
				{
					BodyDeletes.push_back(PhysicsGraphNodeIdToBodyIndex(DeletedNode));
				}
			}
		}
	}
	ed::EndDelete();

	std::sort(ConstraintDeletes.begin(), ConstraintDeletes.end(), std::greater<int32>());
	ConstraintDeletes.erase(std::unique(ConstraintDeletes.begin(), ConstraintDeletes.end()), ConstraintDeletes.end());
	for (int32 ConstraintIndex : ConstraintDeletes)
	{
		if (Asset->RemoveConstraintAt(ConstraintIndex))
		{
			if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint && Selection.ConstraintIndex == ConstraintIndex)
			{
				ClearSelection();
			}
			MarkDirty();
		}
	}

	std::sort(BodyDeletes.begin(), BodyDeletes.end(), std::greater<int32>());
	BodyDeletes.erase(std::unique(BodyDeletes.begin(), BodyDeletes.end()), BodyDeletes.end());
	for (int32 BodyIndex : BodyDeletes)
	{
		if (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Asset->GetBodySetups().size()))
		{
			SelectBody(BodyIndex, true);
			if (DeleteSelection(Asset))
			{
				bGraphPositionsPushed = false;
				MarkDirty();
			}
		}
	}

	const bool bGraphHasUserFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		|| ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	const bool bGraphSelectionChanged = ed::HasSelectionChanged();
	if (bSuppressNextGraphSelectionEvent)
	{
		bSuppressNextGraphSelectionEvent = false;
	}
	else if (bGraphHasUserFocus && bGraphSelectionChanged)
	{
		ed::LinkId SelectedLinks[1];
		const int32 SelectedLinkCount = ed::GetSelectedLinks(SelectedLinks, 1);
		if (SelectedLinkCount > 0)
		{
			const int32 ConstraintIndex = PhysicsGraphLinkIdToConstraintIndex(SelectedLinks[0]);
			if (ConstraintIndex >= 0 && ConstraintIndex < static_cast<int32>(Asset->GetConstraintTemplates().size()))
			{
				SelectConstraint(ConstraintIndex, true);
			}
		}
		else
		{
			ed::NodeId SelectedNodes[1];
			const int32 SelectedNodeCount = ed::GetSelectedNodes(SelectedNodes, 1);
			if (SelectedNodeCount > 0)
			{
				const int32 ConstraintIndex = PhysicsGraphNodeIdToConstraintIndex(SelectedNodes[0]);
				if (ConstraintIndex >= 0 && ConstraintIndex < static_cast<int32>(Asset->GetConstraintTemplates().size()))
				{
					SelectConstraint(ConstraintIndex, true);
				}
				else
				{
					const int32 BodyIndex = PhysicsGraphNodeIdToBodyIndex(SelectedNodes[0]);
					if (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size()))
					{
						SelectBody(BodyIndex, true);
					}
				}
			}
		}
	}

	if (!bSelectionDrivenGraph)
	{
		for (int32 BodyIndex : GraphBodyIndices)
		{
			const UBodySetup* Body = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size())) ? Bodies[BodyIndex] : nullptr;
			if (!Body)
			{
				continue;
			}
			const ImVec2 NodePos = ed::GetNodePosition(ToPhysicsGraphNodeId(BodyIndex));
			float OldX = 0.0f;
			float OldY = 0.0f;
			if (Asset->GetGraphNodePosition(Body->GetBoneName(), OldX, OldY)
				&& (std::fabs(OldX - NodePos.x) > 0.5f || std::fabs(OldY - NodePos.y) > 0.5f))
			{
				MarkDirty();
			}
			Asset->SetGraphNodePosition(Body->GetBoneName(), NodePos.x, NodePos.y);
		}
	}

	ed::NodeId ContextNodeId = 0;
	ed::LinkId ContextLinkId = 0;
	ed::Suspend();
	if (ed::ShowNodeContextMenu(&ContextNodeId))
	{
		ImGui::OpenPopup("PhysicsGraphNodeContext");
	}
	else if (ed::ShowLinkContextMenu(&ContextLinkId))
	{
		ImGui::OpenPopup("PhysicsGraphLinkContext");
	}
	else if (ed::ShowBackgroundContextMenu())
	{
		ImGui::OpenPopup("PhysicsGraphBackgroundContext");
	}

	if (ImGui::BeginPopup("PhysicsGraphNodeContext"))
	{
		const int32 BodyIndex = PhysicsGraphNodeIdToBodyIndex(ContextNodeId);
		if (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Asset->GetBodySetups().size()))
		{
			SelectBody(BodyIndex, true);
			const UBodySetup* Body = Asset->GetBodySetups()[BodyIndex];
			const bool bCollisionEnabled = Body && Asset->IsBodyCollisionEnabled(Body->GetBoneName());
			if (ImGui::MenuItem("Enable Collision", nullptr, bCollisionEnabled, !bCollisionEnabled))
			{
				SetSelectedCollisionEnabled(Asset, true);
				MarkDirty();
			}
			if (ImGui::MenuItem("Disable Collision", nullptr, !bCollisionEnabled, bCollisionEnabled))
			{
				SetSelectedCollisionEnabled(Asset, false);
				MarkDirty();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Body"))
			{
				if (DeleteSelection(Asset))
				{
					bGraphPositionsPushed = false;
					MarkDirty();
				}
			}
		}
		else
		{
			const int32 ConstraintIndex = PhysicsGraphNodeIdToConstraintIndex(ContextNodeId);
			if (ConstraintIndex >= 0 && ConstraintIndex < static_cast<int32>(Asset->GetConstraintTemplates().size()))
			{
				SelectConstraint(ConstraintIndex, true);
				const UPhysicsConstraintTemplate* Constraint = Asset->GetConstraintTemplates()[ConstraintIndex];
				const bool bCollisionDisabled = Constraint
					&& Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
				if (ImGui::MenuItem("Disable Collision Between Bodies", nullptr, bCollisionDisabled, !bCollisionDisabled))
				{
					SetSelectedCollisionEnabled(Asset, false);
					MarkDirty();
				}
				if (ImGui::MenuItem("Enable Collision Between Bodies", nullptr, !bCollisionDisabled, bCollisionDisabled))
				{
					SetSelectedCollisionEnabled(Asset, true);
					MarkDirty();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Constraint"))
				{
					if (Asset->RemoveConstraintAt(ConstraintIndex))
					{
						ClearSelection();
						MarkDirty();
					}
				}
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("PhysicsGraphLinkContext"))
	{
		const int32 ConstraintIndex = PhysicsGraphLinkIdToConstraintIndex(ContextLinkId);
		if (ConstraintIndex >= 0 && ConstraintIndex < static_cast<int32>(Asset->GetConstraintTemplates().size()))
		{
			SelectConstraint(ConstraintIndex, true);
			const UPhysicsConstraintTemplate* Constraint = Asset->GetConstraintTemplates()[ConstraintIndex];
			const bool bCollisionDisabled = Constraint
				&& Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
			if (ImGui::MenuItem("Disable Collision Between Bodies", nullptr, bCollisionDisabled, !bCollisionDisabled))
			{
				SetSelectedCollisionEnabled(Asset, false);
				MarkDirty();
			}
			if (ImGui::MenuItem("Enable Collision Between Bodies", nullptr, !bCollisionDisabled, bCollisionDisabled))
			{
				SetSelectedCollisionEnabled(Asset, true);
				MarkDirty();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Constraint"))
			{
				if (Asset->RemoveConstraintAt(ConstraintIndex))
				{
					ClearSelection();
					MarkDirty();
				}
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("PhysicsGraphBackgroundContext"))
	{
		ImGui::TextDisabled("Drag from a Body output pin to another Body input pin to create a constraint.");
		if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone && ImGui::MenuItem("Create Body From Selected Bone"))
		{
			if (CreateBodyForBone(Asset, Selection.BoneIndex))
			{
				bGraphPositionsPushed = false;
			}
		}
		ImGui::EndPopup();
	}
	ed::Resume();

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderDetailsPanel(UPhysicsAsset* Asset)
{
	DrawPhysicsPanelHeader("Details");
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.045f, 0.045f, 0.045f, 1.0f));
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##PhysicsAssetDetailsFilter", "Search", DetailsFilter, sizeof(DetailsFilter));
	ImGui::PopStyleColor();
	ImGui::Spacing();

	if (!Asset)
	{
		return;
	}

	const FString Filter = DetailsFilter;

	if (ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bAssetChanged = false;
		if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsAssetTable"))
		{
			DrawDetailsTextRow("Physics Asset", IsValidAssetPath(Asset->GetSourcePath()) ? Asset->GetSourcePath() : "Unsaved", Filter);

			FString PreviewMeshPath = Asset->GetPreviewSkeletalMeshPath();
			if (DrawDetailsStringRow("Preview Mesh", PreviewMeshPath, Filter))
			{
				if (PreviewMeshPath.empty())
				{
					PreviewMeshPath = "None";
				}
				Asset->SetPreviewSkeletalMeshPath(PreviewMeshPath);
				bAssetChanged = true;
			}

			DrawDetailsTextRow("Bodies", std::to_string(Asset->GetBodySetups().size()), Filter);
			DrawDetailsTextRow("Constraints", std::to_string(Asset->GetConstraintTemplates().size()), Filter);
			DrawDetailsTextRow("Disabled Collision Pairs", std::to_string(Asset->GetDisabledCollisionPairs().size()), Filter);
			EndPhysicsDetailsTable();
		}

		if (!PreviewMesh)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Preview mesh is not resolved.");
		}
		if (ImGui::Button("Apply Preview Mesh##PhysicsAssetDetailsApplyPreviewMesh"))
		{
			ReloadPreviewMeshFromAsset(Asset);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Preview Mesh##PhysicsAssetDetailsClearPreviewMesh"))
		{
			Asset->SetPreviewSkeletalMeshPath("None");
			ReloadPreviewMeshFromAsset(Asset);
			MarkDirty();
		}
		if (bAssetChanged)
		{
			MarkDirty();
		}
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::None)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("Select a bone, body, shape, or constraint.");
		return;
	}

	ImGui::Spacing();

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone)
	{
		if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
		{
			return;
		}

		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (Selection.BoneIndex < 0 || Selection.BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
		{
			return;
		}

		const FBone& Bone = MeshAsset->Bones[Selection.BoneIndex];
		const FName BoneName(Bone.Name);
		const int32 ExistingBodyIndex = Asset->FindBodyIndex(BoneName);

		if (ImGui::CollapsingHeader("Bone", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsBoneTable"))
			{
				DrawDetailsTextRow("Name", Bone.Name, Filter);
				DrawDetailsTextRow("Index", std::to_string(Selection.BoneIndex), Filter);
				DrawDetailsTextRow("Parent Index", std::to_string(Bone.ParentIndex), Filter);
				DrawDetailsTextRow("Has Body", ExistingBodyIndex >= 0 ? "True" : "False", Filter);
				EndPhysicsDetailsTable();
			}

			if (ExistingBodyIndex >= 0)
			{
				if (ImGui::Button("Select Body"))
				{
					SelectBody(ExistingBodyIndex);
				}
			}
			else if (ImGui::Button("Create Body"))
			{
				CreateBodyForBone(Asset, Selection.BoneIndex);
			}
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex >= 0 && Selection.BodyIndex < static_cast<int32>(Bodies.size()) && Bodies[Selection.BodyIndex])
		{
			UBodySetup* Body = Bodies[Selection.BodyIndex];
			const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
			const FName BoneName = Body->GetBoneName();
			auto AddShapeToSelectedBody = [&](const FPhysicsAssetBodyShapeDesc& ShapeDesc, EPhysicsAssetShapeType ShapeType)
			{
				UBodySetup* EditedBody = nullptr;
				if (MeshAsset)
				{
					EditedBody = FPhysicsAssetEditingLibrary::AddPrimitiveToBone(
						*Asset,
						*MeshAsset,
						Body->GetBoneName(),
						ShapeDesc,
						MapAngularMode(BodyCreationParams.AngularConstraintMode));
				}
				else
				{
					switch (ShapeDesc.PrimitiveType)
					{
					case EPhysicsAssetPrimitiveType::Sphere:
						Body->AddSphere(ShapeDesc.Center, ShapeDesc.Extents.X);
						EditedBody = Body;
						break;
					case EPhysicsAssetPrimitiveType::Box:
						Body->AddBox(ShapeDesc.Center, ShapeDesc.Rotation, ShapeDesc.Extents);
						EditedBody = Body;
						break;
					case EPhysicsAssetPrimitiveType::Capsule:
						Body->AddSphyl(ShapeDesc.Center, ShapeDesc.Rotation, ShapeDesc.Extents.X, ShapeDesc.Extents.Z);
						EditedBody = Body;
						break;
					}
				}

				if (EditedBody)
				{
					const int32 NewBodyIndex = Asset->FindBodyIndex(EditedBody->GetBoneName());
					SelectShape(NewBodyIndex, ShapeType, EditedBody->GetShapeCount(ShapeType) - 1);
					MarkDirty();
				}
			};

			if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool bChanged = false;
				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsBodyPhysicsTable"))
				{
					bool bCollisionEnabled = Asset->IsBodyCollisionEnabled(BoneName);
					if (DrawDetailsBoolRow("Collision Enabled", bCollisionEnabled, Filter))
					{
						Asset->SetBodyCollisionEnabled(BoneName, bCollisionEnabled);
						bChanged = true;
					}

					bool bEnableGravity = Asset->IsBodyGravityEnabled(BoneName);
					if (DrawDetailsBoolRow("Enable Gravity", bEnableGravity, Filter))
					{
						Asset->SetBodyGravityEnabled(BoneName, bEnableGravity);
						bChanged = true;
					}

					bool bConsiderForBounds = Asset->IsBodyConsideredForBounds(BoneName);
					if (DrawDetailsBoolRow("Consider For Bounds", bConsiderForBounds, Filter))
					{
						Asset->SetBodyConsideredForBounds(BoneName, bConsiderForBounds);
						bChanged = true;
					}

					float Mass = Asset->GetBodyMass(BoneName);
					if (DrawDetailsFloatRow("Mass (kg)", Mass, 0.1f, 0.001f, 100000.0f, Filter))
					{
						Asset->SetBodyMass(BoneName, Mass);
						bChanged = true;
					}

					float LinearDamping = Asset->GetBodyLinearDamping(BoneName);
					if (DrawDetailsFloatRow("Linear Damping", LinearDamping, 0.01f, 0.0f, 1000.0f, Filter))
					{
						Asset->SetBodyLinearDamping(BoneName, LinearDamping);
						bChanged = true;
					}

					float AngularDamping = Asset->GetBodyAngularDamping(BoneName);
					if (DrawDetailsFloatRow("Angular Damping", AngularDamping, 0.01f, 0.0f, 1000.0f, Filter))
					{
						Asset->SetBodyAngularDamping(BoneName, AngularDamping);
						bChanged = true;
					}

					EndPhysicsDetailsTable();
				}

				ImGui::TextUnformatted("Physical Material");
				const FString CurrentMaterialPath = Asset->GetBodyPhysicalMaterialPath(BoneName);
				const FString CurrentMaterialLabel = MakePhysicalMaterialDisplayName(CurrentMaterialPath);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##BodyPhysicalMaterialDetails", CurrentMaterialLabel.c_str()))
				{
					FString SelectedMaterialPath;
					if (DrawPhysicalMaterialPickerOptions(CurrentMaterialPath, SelectedMaterialPath))
					{
						Asset->SetBodyPhysicalMaterialPath(BoneName, SelectedMaterialPath);
						bChanged = true;
					}
					if (IsValidAssetPath(CurrentMaterialPath))
					{
						ImGui::Separator();
						ImGui::TextDisabled("%s", CurrentMaterialPath.c_str());
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("Current asset reference");
						}
					}
					ImGui::EndCombo();
				}

				if (bChanged)
				{
					MarkDirty();
				}
			}

			if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const int32 NumSpheres = Body->GetShapeCount(EPhysicsAssetShapeType::Sphere);
				const int32 NumBoxes = Body->GetShapeCount(EPhysicsAssetShapeType::Box);
				const int32 NumCapsules = Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl);
				const int32 NumConvex = Body->GetShapeCount(EPhysicsAssetShapeType::Convex);
				const int32 NumSimpleShapes = NumSpheres + NumBoxes + NumCapsules;
				EPhysicsAssetShapeType SingleShapeType = EPhysicsAssetShapeType::Sphere;
				int32 SingleShapeIndex = -1;
				if (NumSimpleShapes == 1 && NumConvex == 0)
				{
					if (NumSpheres == 1)
					{
						SingleShapeType = EPhysicsAssetShapeType::Sphere;
						SingleShapeIndex = 0;
					}
					else if (NumBoxes == 1)
					{
						SingleShapeType = EPhysicsAssetShapeType::Box;
						SingleShapeIndex = 0;
					}
					else
					{
						SingleShapeType = EPhysicsAssetShapeType::Sphyl;
						SingleShapeIndex = 0;
					}
				}

				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsBodySetupTable"))
				{
					DrawDetailsTextRow("Bone Name", BoneName.ToString(), Filter);
					if (SingleShapeIndex >= 0)
					{
						int32 ShapeTypeValue = SingleShapeType == EPhysicsAssetShapeType::Sphere ? 0
							: SingleShapeType == EPhysicsAssetShapeType::Box ? 1
							: 2;
						const char* ShapeTypeItems[] = { "Sphere", "Box", "Capsule" };
						if (DrawDetailsEnumRow("Type", ShapeTypeValue, ShapeTypeItems, 3, Filter))
						{
							const EPhysicsAssetShapeType NewShapeType = ShapeTypeValue == 0
								? EPhysicsAssetShapeType::Sphere
								: ShapeTypeValue == 1
									? EPhysicsAssetShapeType::Box
									: EPhysicsAssetShapeType::Sphyl;
							SelectShape(Selection.BodyIndex, SingleShapeType, SingleShapeIndex);
							if (ConvertSelectedShape(Body, NewShapeType))
							{
								MarkDirty();
								EndPhysicsDetailsTable();
								return;
							}
						}
					}
					else
					{
						DrawDetailsTextRow("Type", NumSimpleShapes > 1 || NumConvex > 0 ? "Multiple" : "None", Filter);
					}
					DrawDetailsTextRow("Spheres", std::to_string(NumSpheres), Filter);
					DrawDetailsTextRow("Boxes", std::to_string(NumBoxes), Filter);
					DrawDetailsTextRow("Capsules", std::to_string(NumCapsules), Filter);
					DrawDetailsTextRow("Convex", std::to_string(NumConvex), Filter);
					EndPhysicsDetailsTable();
				}

				if (ImGui::Button("Add Sphere"))
				{
					AddShapeToSelectedBody(
						FPhysicsAssetBodyShapeDesc::MakeSphere(FVector::ZeroVector, 0.2f),
						EPhysicsAssetShapeType::Sphere);
				}
				ImGui::SameLine();
				if (ImGui::Button("Add Box"))
				{
					AddShapeToSelectedBody(
						FPhysicsAssetBodyShapeDesc::MakeBox(FVector::ZeroVector, FQuat::Identity, FVector(0.2f, 0.2f, 0.2f)),
						EPhysicsAssetShapeType::Box);
				}
				if (ImGui::Button("Add Capsule"))
				{
					AddShapeToSelectedBody(
						FPhysicsAssetBodyShapeDesc::MakeCapsule(FVector::ZeroVector, FQuat::Identity, 0.15f, 0.5f),
						EPhysicsAssetShapeType::Sphyl);
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete Body"))
				{
					if (DeleteSelection(Asset))
					{
						MarkDirty();
					}
				}
			}
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex >= 0 && Selection.BodyIndex < static_cast<int32>(Bodies.size()) && Bodies[Selection.BodyIndex])
		{
			RenderShapeDetails(Asset, Bodies[Selection.BodyIndex]);
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
	{
		RenderConstraintDetails(Asset);
	}
}

void FPhysicsAssetEditorWidget::RenderShapeDetails(UPhysicsAsset* Asset, UBodySetup* Body)
{
	if (!Asset || !Body)
	{
		return;
	}

	FKAggregateGeom& Geom = Body->GetAggGeom();
	bool bChanged = false;
	const FString Filter = DetailsFilter;

	if (ImGui::CollapsingHeader("Primitive", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsPrimitiveInfoTable"))
		{
			DrawDetailsTextRow("Body", Body->GetBoneName().ToString(), Filter);
			DrawDetailsTextRow("Shape", FString(ShapeTypeLabel(Selection.ShapeType)) + " " + std::to_string(Selection.ShapeIndex), Filter);
			int32 ShapeTypeValue = Selection.ShapeType == EPhysicsAssetShapeType::Sphere ? 0
				: Selection.ShapeType == EPhysicsAssetShapeType::Box ? 1
				: Selection.ShapeType == EPhysicsAssetShapeType::Sphyl ? 2
				: -1;
			const char* ShapeTypeItems[] = { "Sphere", "Box", "Capsule" };
			if (ShapeTypeValue >= 0 && DrawDetailsEnumRow("Type", ShapeTypeValue, ShapeTypeItems, 3, Filter))
			{
				const EPhysicsAssetShapeType NewShapeType = ShapeTypeValue == 0
					? EPhysicsAssetShapeType::Sphere
					: ShapeTypeValue == 1
						? EPhysicsAssetShapeType::Box
						: EPhysicsAssetShapeType::Sphyl;
				if (ConvertSelectedShape(Body, NewShapeType))
				{
					MarkDirty();
					EndPhysicsDetailsTable();
					return;
				}
			}

			bool bCollisionEnabled = Asset->IsBodyCollisionEnabled(Body->GetBoneName());
			if (DrawDetailsBoolRow("Body Collision Enabled", bCollisionEnabled, Filter))
			{
				Asset->SetBodyCollisionEnabled(Body->GetBoneName(), bCollisionEnabled);
				bChanged = true;
			}
			EndPhysicsDetailsTable();
		}

		if (Selection.ShapeType == EPhysicsAssetShapeType::Sphere)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.SphereElems.size()))
			{
				FKSphereElem& Sphere = Geom.SphereElems[Selection.ShapeIndex];
				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsSphereTable"))
				{
					bChanged |= DrawDetailsVec3Row("Center", Sphere.Center, 0.01f, Filter);
					bChanged |= DrawDetailsFloatRow("Radius", Sphere.Radius, 0.01f, 0.001f, 1000.0f, Filter);
					EndPhysicsDetailsTable();
				}
				Sphere.Radius = Clamp(Sphere.Radius, 0.001f, 1000.0f);
			}
		}
		else if (Selection.ShapeType == EPhysicsAssetShapeType::Box)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.BoxElems.size()))
			{
				FKBoxElem& Box = Geom.BoxElems[Selection.ShapeIndex];
				FVector Euler = FRotator::FromQuaternion(Box.Rotation).ToVector();
				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsBoxTable"))
				{
					bChanged |= DrawDetailsVec3Row("Center", Box.Center, 0.01f, Filter);
					if (DrawDetailsVec3Row("Rotation", Euler, 0.25f, Filter))
					{
						Box.Rotation = FRotator(Euler).ToQuaternion();
						bChanged = true;
					}
					bChanged |= DrawDetailsVec3Row("Extents", Box.Extents, 0.01f, Filter);
					EndPhysicsDetailsTable();
				}
				Box.Extents.X = Clamp(Box.Extents.X, 0.001f, 1000.0f);
				Box.Extents.Y = Clamp(Box.Extents.Y, 0.001f, 1000.0f);
				Box.Extents.Z = Clamp(Box.Extents.Z, 0.001f, 1000.0f);
			}
		}
		else if (Selection.ShapeType == EPhysicsAssetShapeType::Sphyl)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.SphylElems.size()))
			{
				FKSphylElem& Capsule = Geom.SphylElems[Selection.ShapeIndex];
				FVector Euler = FRotator::FromQuaternion(Capsule.Rotation).ToVector();
				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsCapsuleTable"))
				{
					bChanged |= DrawDetailsVec3Row("Center", Capsule.Center, 0.01f, Filter);
					if (DrawDetailsVec3Row("Rotation", Euler, 0.25f, Filter))
					{
						Capsule.Rotation = FRotator(Euler).ToQuaternion();
						bChanged = true;
					}
					bChanged |= DrawDetailsFloatRow("Radius", Capsule.Radius, 0.01f, 0.001f, 1000.0f, Filter);
					bChanged |= DrawDetailsFloatRow("Length", Capsule.Length, 0.01f, 0.001f, 1000.0f, Filter);
					EndPhysicsDetailsTable();
				}
				Capsule.Radius = Clamp(Capsule.Radius, 0.001f, 1000.0f);
				Capsule.Length = Clamp(Capsule.Length, 0.001f, 1000.0f);
			}
		}
		else
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.ConvexElems.size()))
			{
				FKConvexElem& Convex = Geom.ConvexElems[Selection.ShapeIndex];
				if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsConvexTable"))
				{
					DrawDetailsTextRow("Vertices", std::to_string(Convex.VertexData.size()), Filter);
					EndPhysicsDetailsTable();
				}
				if (!Convex.VertexData.empty())
				{
					FVector Min = Convex.VertexData[0];
					FVector Max = Convex.VertexData[0];
					for (const FVector& Vertex : Convex.VertexData)
					{
						Min.X = (std::min)(Min.X, Vertex.X);
						Min.Y = (std::min)(Min.Y, Vertex.Y);
						Min.Z = (std::min)(Min.Z, Vertex.Z);
						Max.X = (std::max)(Max.X, Vertex.X);
						Max.Y = (std::max)(Max.Y, Vertex.Y);
						Max.Z = (std::max)(Max.Z, Vertex.Z);
					}

					FVector Center = (Min + Max) * 0.5f;
					const FVector Extents = (Max - Min) * 0.5f;
					if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsConvexBoundsTable"))
					{
						if (DrawDetailsVec3Row("Center", Center, 0.01f, Filter))
						{
							const FVector OldCenter = (Min + Max) * 0.5f;
							const FVector Delta = Center - OldCenter;
							for (FVector& Vertex : Convex.VertexData)
							{
								Vertex += Delta;
							}
							bChanged = true;
						}
						DrawDetailsTextRow("Extents", FString(std::to_string(Extents.X) + " " + std::to_string(Extents.Y) + " " + std::to_string(Extents.Z)), Filter);
						EndPhysicsDetailsTable();
					}

					float UniformScale = 1.0f;
					if (ImGui::DragFloat("Scale Delta", &UniformScale, 0.01f, 0.01f, 100.0f))
					{
						const FVector ScaleCenter = (Min + Max) * 0.5f;
						for (FVector& Vertex : Convex.VertexData)
						{
							Vertex = ScaleCenter + (Vertex - ScaleCenter) * UniformScale;
						}
						bChanged = true;
					}
				}
			}
		}

		if (ImGui::Button("Delete Shape"))
		{
			if (Body->RemoveShape(Selection.ShapeType, Selection.ShapeIndex))
			{
				SelectBody(Selection.BodyIndex);
				MarkDirty();
				return;
			}
		}
	}

	if (bChanged)
	{
		MarkDirty();
	}
}

bool FPhysicsAssetEditorWidget::ConvertSelectedShape(UBodySetup* Body, EPhysicsAssetShapeType NewShapeType)
{
	if (!Body || Selection.Type != EPhysicsAssetEditorSelectionType::Shape)
	{
		return false;
	}
	if (Selection.ShapeType == NewShapeType)
	{
		return false;
	}
	if (NewShapeType != EPhysicsAssetShapeType::Sphere
		&& NewShapeType != EPhysicsAssetShapeType::Box
		&& NewShapeType != EPhysicsAssetShapeType::Sphyl)
	{
		return false;
	}

	FPhysicsAssetPrimitiveSnapshot Snapshot;
	if (!CapturePrimitiveSnapshot(Body, Selection.ShapeType, Selection.ShapeIndex, Snapshot))
	{
		return false;
	}

	if (!Body->RemoveShape(Selection.ShapeType, Selection.ShapeIndex))
	{
		return false;
	}

	switch (NewShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		Body->AddSphere(Snapshot.Center, Snapshot.SphereRadius);
		break;
	case EPhysicsAssetShapeType::Box:
		Body->AddBox(Snapshot.Center, Snapshot.Rotation, Snapshot.BoxExtents);
		break;
	case EPhysicsAssetShapeType::Sphyl:
		Body->AddSphyl(Snapshot.Center, Snapshot.Rotation, Snapshot.CapsuleRadius, Snapshot.CapsuleLength);
		break;
	default:
		return false;
	}

	SelectShape(Selection.BodyIndex, NewShapeType, Body->GetShapeCount(NewShapeType) - 1);
	return true;
}

void FPhysicsAssetEditorWidget::RenderConstraintDetails(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	if (Selection.ConstraintIndex < 0
		|| Selection.ConstraintIndex >= static_cast<int32>(Constraints.size())
		|| !Constraints[Selection.ConstraintIndex])
	{
		return;
	}

	UPhysicsConstraintTemplate* Constraint = Constraints[Selection.ConstraintIndex];
	const FString Filter = DetailsFilter;
	if (ImGui::CollapsingHeader("Constraint", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bChanged = false;
		if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsConstraintTable"))
		{
			DrawDetailsTextRow("Parent Bone", Constraint->GetParentBoneName().ToString(), Filter);
			DrawDetailsTextRow("Child Bone", Constraint->GetChildBoneName().ToString(), Filter);

			bool bDisableCollision = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
			if (DrawDetailsBoolRow("Disable Collision Between Bodies", bDisableCollision, Filter))
			{
				Asset->SetCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName(), bDisableCollision);
				bChanged = true;
			}

			const char* Modes[] = { "Free", "Limited", "Locked" };
			int32 Mode = static_cast<int32>(Constraint->GetAngularMode());
			if (DrawDetailsEnumRow("Angular Mode", Mode, Modes, 3, Filter))
			{
				Constraint->SetAngularMode(static_cast<EAngularConstraintMode>(Mode));
				bChanged = true;
			}

			float Swing1 = Constraint->GetSwing1Limit();
			float Swing2 = Constraint->GetSwing2Limit();
			float Twist = Constraint->GetTwistLimit();
			bool bLimitsChanged = false;
			bLimitsChanged |= DrawDetailsFloatRow("Swing 1 Limit", Swing1, 0.25f, 0.0f, 180.0f, Filter);
			bLimitsChanged |= DrawDetailsFloatRow("Swing 2 Limit", Swing2, 0.25f, 0.0f, 180.0f, Filter);
			bLimitsChanged |= DrawDetailsFloatRow("Twist Limit", Twist, 0.25f, 0.0f, 180.0f, Filter);
			if (bLimitsChanged)
			{
				Constraint->SetAngularLimits(Swing1, Swing2, Twist);
				bChanged = true;
			}

			EndPhysicsDetailsTable();
		}

		if (ImGui::Button("Ball & Socket"))
		{
			if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 45.0f, 45.0f, 180.0f))
			{
				bChanged = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Hinge"))
		{
			if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 0.0f, 0.0f, 90.0f))
			{
				bChanged = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Fixed"))
		{
			if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Locked, 0.0f, 0.0f, 0.0f))
			{
				bChanged = true;
			}
		}

		if (ImGui::CollapsingHeader("Constraint Frames", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FTransform FrameA = Constraint->GetLocalFrameA();
			FTransform FrameB = Constraint->GetLocalFrameB();
			FVector FrameAEuler = FRotator::FromQuaternion(FrameA.Rotation).ToVector();
			FVector FrameBEuler = FRotator::FromQuaternion(FrameB.Rotation).ToVector();

			if (BeginPhysicsDetailsTable("##PhysicsAssetDetailsConstraintFramesTable"))
			{
				if (DrawDetailsVec3Row("Frame A Location", FrameA.Location, 0.01f, Filter))
				{
					Constraint->SetLocalFrameA(FrameA);
					bChanged = true;
				}
				if (DrawDetailsVec3Row("Frame A Rotation", FrameAEuler, 0.25f, Filter))
				{
					FrameA.Rotation = FRotator(FrameAEuler).ToQuaternion();
					Constraint->SetLocalFrameA(FrameA);
					bChanged = true;
				}
				if (DrawDetailsVec3Row("Frame B Location", FrameB.Location, 0.01f, Filter))
				{
					Constraint->SetLocalFrameB(FrameB);
					bChanged = true;
				}
				if (DrawDetailsVec3Row("Frame B Rotation", FrameBEuler, 0.25f, Filter))
				{
					FrameB.Rotation = FRotator(FrameBEuler).ToQuaternion();
					Constraint->SetLocalFrameB(FrameB);
					bChanged = true;
				}
				EndPhysicsDetailsTable();
			}
		}

		if (ImGui::Button("Delete Constraint"))
		{
			if (Asset->RemoveConstraintAt(Selection.ConstraintIndex))
			{
				ClearSelection();
				MarkDirty();
			}
		}

		if (bChanged)
		{
			MarkDirty();
		}
	}
}

bool FPhysicsAssetEditorWidget::CreateBodyForBone(UPhysicsAsset* Asset, int32 BoneIndex)
{
	if (!Asset || !PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
	{
		return false;
	}

	const FName BoneName(MeshAsset->Bones[BoneIndex].Name);
	FPhysicsAssetBodyShapeDesc ShapeDesc;
	if (!MakeDefaultBoneShapeDesc(*MeshAsset, BoneIndex, BodyCreationParams.PrimitiveType, ShapeDesc))
	{
		return false;
	}

	UBodySetup* Body = FPhysicsAssetEditingLibrary::AddPrimitiveToBone(
		*Asset,
		*MeshAsset,
		BoneName,
		ShapeDesc,
		MapAngularMode(BodyCreationParams.AngularConstraintMode));
	if (!Body)
	{
		return false;
	}

	SelectBody(Asset->FindBodyIndex(Body->GetBoneName()));
	bGraphPositionsPushed = false;
	MarkDirty();
	return true;
}

void FPhysicsAssetEditorWidget::RenderToolsPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetToolsPanel", Size, true);
	if (ImGui::BeginTabBar("##PhysicsAssetRightTabs"))
	{
		if (ImGui::BeginTabItem("Tools"))
		{
			DrawPhysicsPanelHeader("Body Creation");

			ImGui::DragFloat("Min Bone Size", &BodyCreationParams.MinBoneSize, 0.5f, 0.0f, 1000.0f);
			if (ImGui::TreeNodeEx("Advanced"))
			{
				ImGui::DragFloat("Min Weld Size", &BodyCreationParams.MinWeldSize, 0.0001f, 0.0f, 1000.0f, "%.4f");
				ImGui::TreePop();
			}

			static const char* const PrimitiveItems[] = { "Box", "Capsule", "Sphere" };
			EnumCombo("Primitive Type", BodyCreationParams.PrimitiveType, PrimitiveItems, 3);

			static const char* const WeightItems[] = { "Any Weight", "Dominant Weight" };
			EnumCombo("Vertex Weighting Type", BodyCreationParams.VertexWeighting, WeightItems, 2);

			ImGui::Checkbox("Auto Orient to Bone", &BodyCreationParams.bAutoOrientToBone);
			ImGui::Checkbox("Create Body for All Bones", &BodyCreationParams.bCreateBodyForAllBones);
			if (BodyCreationParams.bCreateBodyForAllBones)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.32f, 1.0f), "All bones includes cloth/accessory chains.");
			}
			ImGui::InputInt("Lod Index", &BodyCreationParams.LodIndex);

			ImGui::Spacing();
			DrawPhysicsPanelHeader("Constraint Creation");
			ImGui::Checkbox("Create Constraints", &BodyCreationParams.bCreateConstraints);

			static const char* const ConstraintItems[] = { "Free", "Limited", "Locked" };
			EnumCombo("Angular Constraint Mode", BodyCreationParams.AngularConstraintMode, ConstraintItems, 3);

			ImGui::Spacing();
			if (ImGui::Button("Re-generate Bodies", ImVec2(-1.0f, 26.0f)))
			{
				RegenerateBodies(Asset);
			}

			ImGui::Spacing();
			DrawPhysicsPanelHeader("Selection");
			if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone)
			{
				const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
				if (MeshAsset
					&& Selection.BoneIndex >= 0
					&& Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size()))
				{
					const FName BoneName(MeshAsset->Bones[Selection.BoneIndex].Name);
					const int32 ExistingBodyIndex = Asset->FindBodyIndex(BoneName);
					if (ExistingBodyIndex >= 0)
					{
						if (ImGui::Button("Select Existing Body##ToolsSelectBody", ImVec2(-1.0f, 24.0f)))
						{
							SelectBody(ExistingBodyIndex);
						}
					}
					else if (ImGui::Button("Create Body For Selected Bone##ToolsCreateBody", ImVec2(-1.0f, 24.0f)))
					{
						CreateBodyForBone(Asset, Selection.BoneIndex);
					}
				}
				else
				{
					ImGui::TextDisabled("Selected bone is invalid.");
				}
			}
			else if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
			{
				const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
				UBodySetup* Body = (Selection.BodyIndex >= 0 && Selection.BodyIndex < static_cast<int32>(Bodies.size()))
					? Bodies[Selection.BodyIndex]
					: nullptr;
				if (Body)
				{
					if (ImGui::Button("Add Sphere##ToolsAddSphere", ImVec2(-1.0f, 24.0f)))
					{
						Body->AddSphere(FVector::ZeroVector, 0.2f);
						SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphere, Body->GetShapeCount(EPhysicsAssetShapeType::Sphere) - 1);
						MarkDirty();
					}
					if (ImGui::Button("Add Box##ToolsAddBox", ImVec2(-1.0f, 24.0f)))
					{
						Body->AddBox(FVector::ZeroVector, FQuat::Identity, FVector(0.2f, 0.2f, 0.2f));
						SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Box, Body->GetShapeCount(EPhysicsAssetShapeType::Box) - 1);
						MarkDirty();
					}
					if (ImGui::Button("Add Capsule##ToolsAddCapsule", ImVec2(-1.0f, 24.0f)))
					{
						Body->AddSphyl(FVector::ZeroVector, FQuat::Identity, 0.15f, 0.5f);
						SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphyl, Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl) - 1);
						MarkDirty();
					}
					if (ImGui::Button("Delete Body##ToolsDeleteBody", ImVec2(-1.0f, 24.0f)))
					{
						if (DeleteSelection(Asset))
						{
							MarkDirty();
						}
					}
				}
			}
			else if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
			{
				if (ImGui::Button("Delete Shape##ToolsDeleteShape", ImVec2(-1.0f, 24.0f)))
				{
					if (DeleteSelection(Asset))
					{
						MarkDirty();
					}
				}
				if (ImGui::Button("Select Owning Body##ToolsSelectOwningBody", ImVec2(-1.0f, 24.0f)))
				{
					SelectBody(Selection.BodyIndex);
				}
			}
			else if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
			{
				if (ImGui::Button("Ball & Socket##ToolsBallSocket", ImVec2(-1.0f, 24.0f)))
				{
					if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 45.0f, 45.0f, 180.0f))
					{
						MarkDirty();
					}
				}
				if (ImGui::Button("Hinge##ToolsHinge", ImVec2(-1.0f, 24.0f)))
				{
					if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Limited, 0.0f, 0.0f, 90.0f))
					{
						MarkDirty();
					}
				}
				if (ImGui::Button("Fixed##ToolsFixed", ImVec2(-1.0f, 24.0f)))
				{
					if (ApplyConstraintPreset(Asset, Selection.ConstraintIndex, EAngularConstraintMode::Locked, 0.0f, 0.0f, 0.0f))
					{
						MarkDirty();
					}
				}
				if (ImGui::Button("Delete Constraint##ToolsDeleteConstraint", ImVec2(-1.0f, 24.0f)))
				{
					if (DeleteSelection(Asset))
					{
						MarkDirty();
					}
				}
			}
			else
			{
				ImGui::TextDisabled("Select a bone, body, shape, or constraint.");
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Profiles"))
		{
			DrawPhysicsPanelHeader("Profiles");
			FName SelectedBoneName = FName::None;
			if (GetSelectedBodyBoneName(Asset, SelectedBoneName))
			{
				ImGui::Text("Selected Body: %s", SelectedBoneName.ToString().c_str());
				bool bCollisionEnabled = Asset->IsBodyCollisionEnabled(SelectedBoneName);
				if (ImGui::Checkbox("Collision Enabled", &bCollisionEnabled))
				{
					Asset->SetBodyCollisionEnabled(SelectedBoneName, bCollisionEnabled);
					MarkDirty();
				}
				ImGui::Text("Physical Material");
				const FString CurrentMaterialPath = Asset->GetBodyPhysicalMaterialPath(SelectedBoneName);
				const FString CurrentMaterialLabel = MakePhysicalMaterialDisplayName(CurrentMaterialPath);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##ProfilePhysicalMaterial", CurrentMaterialLabel.c_str()))
				{
					FString SelectedMaterialPath;
					if (DrawPhysicalMaterialPickerOptions(CurrentMaterialPath, SelectedMaterialPath))
					{
						Asset->SetBodyPhysicalMaterialPath(SelectedBoneName, SelectedMaterialPath);
						MarkDirty();
					}
					if (IsValidAssetPath(CurrentMaterialPath))
					{
						ImGui::Separator();
						ImGui::TextDisabled("%s", CurrentMaterialPath.c_str());
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("Current asset reference");
						}
					}
					ImGui::EndCombo();
				}
			}
			else if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
			{
				const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
				if (Selection.ConstraintIndex >= 0 && Selection.ConstraintIndex < static_cast<int32>(Constraints.size()) && Constraints[Selection.ConstraintIndex])
				{
					UPhysicsConstraintTemplate* Constraint = Constraints[Selection.ConstraintIndex];
					ImGui::Text("Selected Constraint");
					ImGui::Text("%s <-> %s",
						Constraint->GetParentBoneName().ToString().c_str(),
						Constraint->GetChildBoneName().ToString().c_str());
					bool bDisabled = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());
					if (ImGui::Checkbox("Disable Collision Pair", &bDisabled))
					{
						Asset->SetCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName(), bDisabled);
						MarkDirty();
					}
				}
			}
			else
			{
				ImGui::TextDisabled("Select a body or constraint.");
			}

			ImGui::Separator();
			DrawPhysicsPanelHeader("Disabled Collision Pairs");
			const TArray<FPhysicsAssetCollisionDisablePair>& DisabledPairs = Asset->GetDisabledCollisionPairs();
			for (int32 PairIndex = 0; PairIndex < static_cast<int32>(DisabledPairs.size()); ++PairIndex)
			{
				const FPhysicsAssetCollisionDisablePair Pair = DisabledPairs[PairIndex];
				ImGui::PushID(PairIndex);
				ImGui::Text("%s <-> %s", Pair.BoneA.ToString().c_str(), Pair.BoneB.ToString().c_str());
				ImGui::SameLine();
				if (ImGui::Button("Remove"))
				{
					Asset->SetCollisionDisabled(Pair.BoneA, Pair.BoneB, false);
					MarkDirty();
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::EndChild();
}

bool FPhysicsAssetEditorWidget::RegenerateBodies(UPhysicsAsset* Asset)
{
	if (!Asset || !PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	Asset->Clear();
	const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
	GeneratePhysicsAssetBodies(*Asset, *MeshAsset, BodyCreationParams);
	GeneratePhysicsAssetConstraints(*Asset, *MeshAsset, BodyCreationParams);
	ClearSelection();
	bGraphPositionsPushed = false;
	MarkDirty();
	return !Asset->GetBodySetups().empty();
}

void FPhysicsAssetEditorWidget::SetEditorMode(EPhysicsAssetEditorMode Mode)
{
	if (ActiveMode == Mode)
	{
		return;
	}

	StopPreviewSimulation();

	if (Mode == EPhysicsAssetEditorMode::Preview)
	{
		ClearSelection();
		ApplyViewPreset(EPhysicsAssetEditorViewPreset::Physics);
		ActiveMode = Mode;
		return;
	}

	ActiveMode = Mode;
	if (Mode == EPhysicsAssetEditorMode::Constraint)
	{
		ViewportClient.SetShowBodies(true);
		ViewportClient.SetShowConstraints(true);
		ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
	}
	else if (Mode == EPhysicsAssetEditorMode::Body)
	{
		ViewportClient.SetShowBodies(true);
		ActiveViewPreset = EPhysicsAssetEditorViewPreset::Custom;
	}
}

void FPhysicsAssetEditorWidget::ApplyViewPreset(EPhysicsAssetEditorViewPreset Preset)
{
	ActiveViewPreset = Preset;

	switch (Preset)
	{
	case EPhysicsAssetEditorViewPreset::Skeletal:
		ViewportClient.SetShowPreviewMesh(true);
		ViewportClient.SetShowSkeleton(false);
		ViewportClient.SetShowBodies(false);
		ViewportClient.SetShowConstraints(false);
		break;

	case EPhysicsAssetEditorViewPreset::Bones:
		ViewportClient.SetShowPreviewMesh(true);
		ViewportClient.SetShowSkeleton(true);
		ViewportClient.SetShowBodies(false);
		ViewportClient.SetShowConstraints(false);
		break;

	case EPhysicsAssetEditorViewPreset::Physics:
		ViewportClient.SetShowPreviewMesh(true);
		ViewportClient.SetShowSkeleton(false);
		ViewportClient.SetShowBodies(true);
		ViewportClient.SetShowConstraints(true);
		break;

	case EPhysicsAssetEditorViewPreset::Custom:
	default:
		break;
	}
}

void FPhysicsAssetEditorWidget::HandleViewportSelectionClick()
{
	if (ActiveMode == EPhysicsAssetEditorMode::Preview || ViewportClient.IsSimulatingPhysics())
	{
		return;
	}
	if (ViewportClient.IsGizmoHolding() || ViewportClient.IsGizmoHandleAtMouse())
	{
		return;
	}

	if (ActiveMode == EPhysicsAssetEditorMode::Constraint)
	{
		int32 ConstraintIndex = -1;
		if (ViewportClient.PickConstraintAtMouse(ConstraintIndex))
		{
			SelectConstraint(ConstraintIndex);
		}
		else
		{
			ClearSelection();
		}
		return;
	}

	FPhysicsAssetEditorHitResult Hit;
	if (ViewportClient.PickBodyShapeAtMouse(Hit))
	{
		SelectShape(Hit.BodyIndex, Hit.ShapeType, Hit.ShapeIndex);
	}
	else
	{
		ClearSelection();
	}
}

void FPhysicsAssetEditorWidget::SelectBody(int32 BodyIndex, bool bFromGraph)
{
	const bool bWasSelectionDrivenGraph = Selection.Type != EPhysicsAssetEditorSelectionType::None && !bSelectionOriginatedFromGraph;
	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex)
	{
		if (!bFromGraph)
		{
			RequestGraphEditorSelectionSync();
		}
		if (bSelectionOriginatedFromGraph != bFromGraph)
		{
			bSelectionOriginatedFromGraph = bFromGraph;
			if (!bFromGraph)
			{
				RequestGraphEditorSelectionSync();
				bGraphPositionsPushed = false;
			}
			else if (bWasSelectionDrivenGraph)
			{
				bGraphPositionsPushed = false;
			}
		}
		return;
	}
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Body;
	Selection.BodyIndex = BodyIndex;
	bSelectionOriginatedFromGraph = bFromGraph;
	if (!bFromGraph)
	{
		RequestGraphEditorSelectionSync();
		bGraphPositionsPushed = false;
	}
	else if (bWasSelectionDrivenGraph)
	{
		bGraphPositionsPushed = false;
	}
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape
		&& Selection.BodyIndex == BodyIndex
		&& Selection.ShapeType == ShapeType
		&& Selection.ShapeIndex == ShapeIndex)
	{
		RequestGraphEditorSelectionSync();
		return;
	}
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Shape;
	Selection.BodyIndex = BodyIndex;
	Selection.ShapeType = ShapeType;
	Selection.ShapeIndex = ShapeIndex;
	bSelectionOriginatedFromGraph = false;
	RequestGraphEditorSelectionSync();
	bGraphPositionsPushed = false;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectConstraint(int32 ConstraintIndex, bool bFromGraph)
{
	const bool bWasSelectionDrivenGraph = Selection.Type != EPhysicsAssetEditorSelectionType::None && !bSelectionOriginatedFromGraph;
	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint && Selection.ConstraintIndex == ConstraintIndex)
	{
		if (!bFromGraph)
		{
			RequestGraphEditorSelectionSync();
		}
		if (bSelectionOriginatedFromGraph != bFromGraph)
		{
			bSelectionOriginatedFromGraph = bFromGraph;
			if (!bFromGraph)
			{
				RequestGraphEditorSelectionSync();
				bGraphPositionsPushed = false;
			}
			else if (bWasSelectionDrivenGraph)
			{
				bGraphPositionsPushed = false;
			}
		}
		return;
	}
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Constraint;
	Selection.ConstraintIndex = ConstraintIndex;
	bSelectionOriginatedFromGraph = bFromGraph;
	if (!bFromGraph)
	{
		RequestGraphEditorSelectionSync();
		bGraphPositionsPushed = false;
	}
	else if (bWasSelectionDrivenGraph)
	{
		bGraphPositionsPushed = false;
	}
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectBone(int32 BoneIndex)
{
	if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone && Selection.BoneIndex == BoneIndex)
	{
		return;
	}
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Bone;
	Selection.BoneIndex = BoneIndex;
	bSelectionOriginatedFromGraph = false;
	ClearGraphEditorSelection();
	bGraphPositionsPushed = false;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::ClearSelection()
{
	if (Selection.Type == EPhysicsAssetEditorSelectionType::None)
	{
		return;
	}
	Selection.Clear();
	bSelectionOriginatedFromGraph = false;
	ClearGraphEditorSelection();
	bGraphPositionsPushed = false;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::ValidateSelection(UPhysicsAsset* Asset)
{
	if (Selection.Type == EPhysicsAssetEditorSelectionType::None)
	{
		return;
	}

	bool bValid = false;
	if (Asset)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();

		switch (Selection.Type)
		{
		case EPhysicsAssetEditorSelectionType::Body:
			bValid = Selection.BodyIndex >= 0
				&& Selection.BodyIndex < static_cast<int32>(Bodies.size())
				&& Bodies[Selection.BodyIndex] != nullptr;
			break;

		case EPhysicsAssetEditorSelectionType::Shape:
			bValid = Selection.BodyIndex >= 0
				&& Selection.BodyIndex < static_cast<int32>(Bodies.size())
				&& Bodies[Selection.BodyIndex] != nullptr
				&& Selection.ShapeIndex >= 0
				&& Selection.ShapeIndex < Bodies[Selection.BodyIndex]->GetShapeCount(Selection.ShapeType);
			break;

		case EPhysicsAssetEditorSelectionType::Constraint:
			bValid = Selection.ConstraintIndex >= 0
				&& Selection.ConstraintIndex < static_cast<int32>(Constraints.size())
				&& Constraints[Selection.ConstraintIndex] != nullptr;
			break;

		case EPhysicsAssetEditorSelectionType::Bone:
			if (PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
			{
				const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
				bValid = Selection.BoneIndex >= 0
					&& Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size());
			}
			break;

		default:
			break;
		}
	}

	if (!bValid)
	{
		ClearSelection();
	}
}

void FPhysicsAssetEditorWidget::SyncViewportHighlight()
{
	switch (Selection.Type)
	{
	case EPhysicsAssetEditorSelectionType::Body:
		ViewportClient.SetHighlightedBody(Selection.BodyIndex);
		ViewportClient.SetGizmoBodySelection(Selection.BodyIndex);
		break;
	case EPhysicsAssetEditorSelectionType::Shape:
		ViewportClient.SetHighlightedShape(Selection.BodyIndex, Selection.ShapeType, Selection.ShapeIndex);
		ViewportClient.SetGizmoShapeSelection(Selection.BodyIndex, Selection.ShapeType, Selection.ShapeIndex);
		break;
	case EPhysicsAssetEditorSelectionType::Constraint:
		ViewportClient.SetHighlightedConstraint(Selection.ConstraintIndex);
		ViewportClient.SetGizmoConstraintSelection(Selection.ConstraintIndex);
		break;
	default:
		ViewportClient.ClearHighlight();
		ViewportClient.ClearGizmoSelection();
		break;
	}
}

bool FPhysicsAssetEditorWidget::DeleteSelection(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex < 0 || Selection.BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[Selection.BodyIndex])
		{
			return false;
		}

		const FName BoneName = Bodies[Selection.BodyIndex]->GetBoneName();
		const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
		const bool bRemoved = MeshAsset
			? FPhysicsAssetEditingLibrary::RemoveBodyAndReconnectConstraints(*Asset, *MeshAsset, BoneName)
			: Asset->RemoveBodySetupAt(Selection.BodyIndex);
		if (bRemoved)
		{
			ClearSelection();
			bGraphPositionsPushed = false;
		}
		return bRemoved;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex < 0 || Selection.BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[Selection.BodyIndex])
		{
			return false;
		}

		const bool bRemoved = Bodies[Selection.BodyIndex]->RemoveShape(Selection.ShapeType, Selection.ShapeIndex);
		if (bRemoved)
		{
			SelectBody(Selection.BodyIndex);
		}
		return bRemoved;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
	{
		const bool bRemoved = Asset->RemoveConstraintAt(Selection.ConstraintIndex);
		if (bRemoved)
		{
			ClearSelection();
		}
		return bRemoved;
	}

	return false;
}
