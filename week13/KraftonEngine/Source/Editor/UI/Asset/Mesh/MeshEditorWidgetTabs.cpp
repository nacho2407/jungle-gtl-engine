#include "MeshEditorWidgetTabs.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include "Animation/AnimInstance.h"
#include "Animation/AnimationManager.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Sequence/AnimDataModel.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Component/Debug/SkeletalMeshDebugComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Physics/IPhysicsScene.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsAssetManager.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"
#include "Serialization/MemoryArchive.h"
#include "Slate/SlateApplication.h"
#include "UI/Asset/Animation/AnimMontagePropertyPanel.h"
#include "UI/Asset/Animation/AnimSequencePropertyPanel.h"
#include "UI/Asset/Animation/AnimationTimelinePanel.h"
#include "UI/Util/EditorFileUtils.h"
#include "Core/Logging/Log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace
{
	FString FormatMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}

	bool IsValidAssetPath(const FString& Path)
	{
		return !Path.empty() && Path != "None";
	}

	FString GetPhysicsAssetSourceMeshPath(const UPhysicsAsset* PhysicsAsset)
	{
		if (!PhysicsAsset)
		{
			return FString();
		}

		if (const USkeletalMesh* SourceMesh = PhysicsAsset->GetTypedOuter<USkeletalMesh>())
		{
			return SourceMesh->GetAssetPathFileName();
		}

		return PhysicsAsset->GetSourceSkeletalMeshPath();
	}

	USkeletalMesh* ResolveSourceMeshForPhysicsAsset(UPhysicsAsset* PhysicsAsset)
	{
		if (!PhysicsAsset)
		{
			return nullptr;
		}

		if (USkeletalMesh* SourceMesh = PhysicsAsset->GetTypedOuter<USkeletalMesh>())
		{
			SourceMesh->SetPhysicsAsset(PhysicsAsset);
			return SourceMesh;
		}

		const FString& SourceMeshPath = PhysicsAsset->GetSourceSkeletalMeshPath();
		if (!IsValidAssetPath(SourceMeshPath))
		{
			return nullptr;
		}

		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* SourceMesh = FMeshManager::LoadSkeletalMesh(SourceMeshPath, Device);
		if (!SourceMesh)
		{
			return nullptr;
		}

		PhysicsAsset->SetOuter(SourceMesh);
		SourceMesh->SetPhysicsAsset(PhysicsAsset);
		return SourceMesh;
	}

	bool IsSameSkeletonBindingForAnimationList(const FSkeletonBinding& A, const FSkeletonBinding& B)
	{
		return A.SkeletonPath == B.SkeletonPath
			&& A.SkeletonAssetGuid == B.SkeletonAssetGuid
			&& A.CompatibilitySignature == B.CompatibilitySignature;
	}

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

	bool RenderAddPhysicsBodyShapeMenu(UBodySetup* BodySetup)
	{
		if (!BodySetup)
		{
			return false;
		}

		FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		if (ImGui::MenuItem("Add Sphere"))
		{
			AggGeom.SphereElems.push_back(FKSphereElem());
			return true;
		}
		if (ImGui::MenuItem("Add Box"))
		{
			AggGeom.BoxElems.push_back(FKBoxElem());
			return true;
		}
		if (ImGui::MenuItem("Add Capsule"))
		{
			AggGeom.SphylElems.push_back(FKSphylElem());
			return true;
		}

		return false;
	}

	bool HasVectorChanged(const FVector& Before, const FVector& After)
	{
		return FVector::Distance(Before, After) > 1.0e-4f;
	}

	void SerializeTransformForSnapshot(FArchive& Ar, const FTransform& Transform)
	{
		FVector Location = Transform.Location;
		FVector Scale = Transform.Scale;
		float RotationX = Transform.Rotation.X;
		float RotationY = Transform.Rotation.Y;
		float RotationZ = Transform.Rotation.Z;
		float RotationW = Transform.Rotation.W;

		Ar << Location;
		Ar << RotationX;
		Ar << RotationY;
		Ar << RotationZ;
		Ar << RotationW;
		Ar << Scale;
	}

	TArray<uint8> CaptureObjectSnapshot(UObject* Object)
	{
		if (!Object)
		{
			return {};
		}

		FMemoryArchive Archive(true);
		Object->Serialize(Archive);
		return Archive.GetBuffer();
	}

	TArray<uint8> CaptureConstraintSnapshot(const FConstraintInstanceInitDesc* ConstraintDesc)
	{
		if (!ConstraintDesc)
		{
			return {};
		}

		FMemoryArchive Archive(true);
		FName ParentBoneName = ConstraintDesc->ParentBoneName;
		FName ChildBoneName = ConstraintDesc->ChildBoneName;
		float TwistLimitDegrees = ConstraintDesc->TwistLimitDegrees;
		float Swing1LimitDegrees = ConstraintDesc->Swing1LimitDegrees;
		float Swing2LimitDegrees = ConstraintDesc->Swing2LimitDegrees;
		bool bEnableCollision = ConstraintDesc->bEnableCollision;
		bool bEnableProjection = ConstraintDesc->bEnableProjection;
		float ProjectionLinearTolerance = ConstraintDesc->ProjectionLinearTolerance;
		float ProjectionAngularToleranceDegrees = ConstraintDesc->ProjectionAngularToleranceDegrees;

		Archive << ParentBoneName;
		Archive << ChildBoneName;
		SerializeTransformForSnapshot(Archive, ConstraintDesc->ParentFrame);
		SerializeTransformForSnapshot(Archive, ConstraintDesc->ChildFrame);
		Archive << TwistLimitDegrees;
		Archive << Swing1LimitDegrees;
		Archive << Swing2LimitDegrees;
		Archive << bEnableCollision;
		Archive << bEnableProjection;
		Archive << ProjectionLinearTolerance;
		Archive << ProjectionAngularToleranceDegrees;

		return Archive.GetBuffer();
	}

	bool IsSameDetailTarget(const FSelectionDetailTarget& A, const FSelectionDetailTarget& B)
	{
		return A.ObjectPtr == B.ObjectPtr
			&& A.StructType == B.StructType
			&& A.ContainerPtr == B.ContainerPtr;
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

	FMorphTargetCurve& FindOrAddMorphCurve(UAnimSequence* Seq, const FString& MorphTargetName)
	{
		TArray<FMorphTargetCurve>& Curves = Seq->GetMutableMorphTargetCurves();
		for (FMorphTargetCurve& Curve : Curves)
		{
			if (Curve.MorphTargetName == MorphTargetName)
			{
				return Curve;
			}
		}
		FMorphTargetCurve NewCurve;
		NewCurve.MorphTargetName = MorphTargetName;
		Curves.push_back(std::move(NewCurve));
		return Curves.back();
	}

	void AddOrUpdateMorphCurveKey(FMorphTargetCurve& Curve, float TimeSeconds, float Value)
	{
		constexpr float TimeTolerance = 1.0e-4f;
		for (FRawFloatCurveKey& Key : Curve.Curve.Keys)
		{
			if (std::fabs(Key.TimeSeconds - TimeSeconds) <= TimeTolerance)
			{
				Key.Value = Value;
				return;
			}
		}
		FRawFloatCurveKey NewKey;
		NewKey.TimeSeconds = TimeSeconds;
		NewKey.Value = Value;
		NewKey.Interpolation = 2;
		Curve.Curve.Keys.push_back(NewKey);
		std::sort(
			Curve.Curve.Keys.begin(),
			Curve.Curve.Keys.end(),
			[](const FRawFloatCurveKey& A, const FRawFloatCurveKey& B)
			{
				return A.TimeSeconds < B.TimeSeconds;
			});
	}

	FString ExtractStem(const FString& Path)
	{
		const size_t LastSlash = Path.find_last_of("/\\");
		const size_t Start = (LastSlash == FString::npos) ? 0 : LastSlash + 1;
		const size_t LastDot = Path.find_last_of('.');
		const size_t End = (LastDot == FString::npos || LastDot < Start) ? Path.size() : LastDot;
		return Path.substr(Start, End - Start);
	}
}

class FPhysicsAssetRagdollPanel
{
public:
	void Reset()
	{
		Stop(nullptr);
		InitialLocalPose.clear();
		InitialRelativeTransform = FTransform();
		bHasInitialRelativeTransform = false;
		bPaused = false;
	}

	void Stop(USkeletalMeshDebugComponent* MeshComponent)
	{
		if (MeshComponent)
		{
			MeshComponent->StopRagdollPreviewSimulation();
			RestoreInitialPose(MeshComponent);
		}
		bSimulationActive = false;
		bPaused = false;
	}

	void Tick(float DeltaTime, USkeletalMeshDebugComponent* MeshComponent)
	{
		if (!bSimulationActive || bPaused || !MeshComponent)
		{
			return;
		}

		if (!MeshComponent->IsRagdollEnabled())
		{
			bSimulationActive = false;
			bPaused = false;
			return;
		}

		if (UWorld* World = MeshComponent->GetWorld())
		{
			if (IPhysicsScene* PhysicsScene = World->GetPhysicsScene())
			{
				PhysicsScene->Tick(DeltaTime);
			}
		}

		MeshComponent->TickRagdollPreviewSimulation(DeltaTime);
	}

	void Render(USkeletalMeshDebugComponent* MeshComponent, UPhysicsAsset* PhysicsAsset, int32 SelectedBodyIndex)
	{
		RenderContents(MeshComponent, PhysicsAsset, SelectedBodyIndex);
	}

	bool IsActive() const { return bSimulationActive; }

private:
	void RenderContents(USkeletalMeshDebugComponent* MeshComponent, UPhysicsAsset* PhysicsAsset, int32 SelectedBodyIndex)
	{
		ImGui::TextUnformatted("Ragdoll");
		ImGui::Separator();

		const bool bCanPlay = MeshComponent && PhysicsAsset;
		if (!bSimulationActive)
		{
			if (!bCanPlay)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("Play", ImVec2(64.0f, 0.0f)))
			{
				Start(MeshComponent, PhysicsAsset);
			}
			if (!bCanPlay)
			{
				ImGui::EndDisabled();
			}
		}
		else if (ImGui::Button("Stop", ImVec2(64.0f, 0.0f)))
		{
			Stop(MeshComponent);
		}

		ImGui::SameLine();
		if (!bSimulationActive)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(bPaused ? "Resume" : "Pause", ImVec2(72.0f, 0.0f)))
		{
			bPaused = !bPaused;
		}
		if (!bSimulationActive)
		{
			ImGui::EndDisabled();
		}

		if (ImGui::Checkbox("Gravity", &bGravityEnabled))
		{
			if (MeshComponent)
			{
				MeshComponent->SetRagdollGravityEnabled(bGravityEnabled);
			}
		}

		ImGui::SameLine();
		if (ImGui::Checkbox("Constraints", &bCreateConstraints))
		{
			if (MeshComponent)
			{
				MeshComponent->SetRagdollCreateConstraints(bCreateConstraints);
			}
		}

		if (ImGui::BeginCombo("Self Collision", GetSelfCollisionModeLabel(SelfCollisionMode)))
		{
			RenderSelfCollisionModeOption("Disable All", ERagdollSelfCollisionMode::DisableAll, MeshComponent);
			RenderSelfCollisionModeOption("No Parent/Child", ERagdollSelfCollisionMode::DisableParentChild, MeshComponent);
			RenderSelfCollisionModeOption("Enable All", ERagdollSelfCollisionMode::EnableAll, MeshComponent);
			ImGui::EndCombo();
		}

		if (ImGui::SliderFloat("Blend", &BlendWeight, 0.0f, 1.0f, "%.2f"))
		{
			if (MeshComponent)
			{
				MeshComponent->SetRagdollGlobalPhysicsBlendWeight(BlendWeight);
			}
		}

		if (!bSimulationActive)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Sim All", ImVec2(96.0f, 0.0f)) && MeshComponent)
		{
			MeshComponent->SetAllBodiesSimulatePhysics(true);
			MeshComponent->SetAllBodiesPhysicsBlendWeight(1.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Kin All", ImVec2(96.0f, 0.0f)) && MeshComponent)
		{
			MeshComponent->SetAllBodiesSimulatePhysics(false);
			MeshComponent->SetAllBodiesPhysicsBlendWeight(0.0f);
		}

		const FName SelectedBoneName = GetSelectedBodyBoneName(PhysicsAsset, SelectedBodyIndex);
		const bool bHasSelectedBody = SelectedBoneName.IsValid();
		if (!bHasSelectedBody)
		{
			ImGui::BeginDisabled();
		}

		ImGui::Checkbox("Include Selected", &bIncludeSelectedSelf);
		if (ImGui::Button("Sim Selected", ImVec2(96.0f, 0.0f)) && MeshComponent && bHasSelectedBody)
		{
			MeshComponent->SetAllBodiesBelowSimulatePhysics(SelectedBoneName, true, bIncludeSelectedSelf);
			MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(SelectedBoneName, 1.0f, bIncludeSelectedSelf);
		}
		ImGui::SameLine();
		if (ImGui::Button("Kin Selected", ImVec2(96.0f, 0.0f)) && MeshComponent && bHasSelectedBody)
		{
			MeshComponent->SetAllBodiesBelowSimulatePhysics(SelectedBoneName, false, bIncludeSelectedSelf);
			MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(SelectedBoneName, 0.0f, bIncludeSelectedSelf);
		}

		if (!bHasSelectedBody)
		{
			ImGui::EndDisabled();
		}
		if (!bSimulationActive)
		{
			ImGui::EndDisabled();
		}
	}

	void Start(USkeletalMeshDebugComponent* MeshComponent, UPhysicsAsset* PhysicsAsset)
	{
		if (!MeshComponent || !PhysicsAsset)
		{
			return;
		}

		CaptureInitialPose(MeshComponent);

		if (USkeletalMesh* Mesh = MeshComponent->GetSkeletalMesh())
		{
			Mesh->SetPhysicsAsset(PhysicsAsset);
		}

		MeshComponent->SetRagdollCreateConstraints(bCreateConstraints);
		MeshComponent->SetRagdollSelfCollisionMode(SelfCollisionMode);
		MeshComponent->SetRagdollGlobalPhysicsBlendWeight(BlendWeight);
		MeshComponent->SetRagdollGravityEnabled(bGravityEnabled);
		MeshComponent->SetRagdollEnabled(true);
		MeshComponent->SetRagdollGravityEnabled(bGravityEnabled);
		MeshComponent->SetRagdollGlobalPhysicsBlendWeight(BlendWeight);

		bSimulationActive = MeshComponent->IsRagdollEnabled();
		bPaused = false;
	}

	void RestoreInitialPose(USkeletalMeshDebugComponent* MeshComponent)
	{
		if (!MeshComponent || InitialLocalPose.empty() || !bHasInitialRelativeTransform)
		{
			return;
		}

		MeshComponent->SetRelativeTransform(InitialRelativeTransform);
		MeshComponent->SetRagdollPreviewLocalPose(InitialLocalPose);
	}

	void CaptureInitialPose(USkeletalMeshDebugComponent* MeshComponent)
	{
		InitialLocalPose.clear();
		InitialRelativeTransform = FTransform();
		bHasInitialRelativeTransform = false;

		USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMesh() : nullptr;
		const FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset)
		{
			return;
		}

		InitialRelativeTransform = MeshComponent->GetRelativeTransform();
		bHasInitialRelativeTransform = true;

		const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
		InitialLocalPose.reserve(BoneCount);
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			InitialLocalPose.push_back(MeshComponent->GetBoneLocalTransformByIndex(BoneIndex));
		}
	}

	static FName GetSelectedBodyBoneName(UPhysicsAsset* PhysicsAsset, int32 SelectedBodyIndex)
	{
		if (!PhysicsAsset || SelectedBodyIndex < 0)
		{
			return FName::None;
		}

		const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
		if (SelectedBodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[SelectedBodyIndex])
		{
			return FName::None;
		}

		return Bodies[SelectedBodyIndex]->BoneName;
	}

	static const char* GetSelfCollisionModeLabel(ERagdollSelfCollisionMode Mode)
	{
		switch (Mode)
		{
		case ERagdollSelfCollisionMode::DisableAll:
			return "Disable All";
		case ERagdollSelfCollisionMode::EnableAll:
			return "Enable All";
		case ERagdollSelfCollisionMode::DisableParentChild:
		default:
			return "No Parent/Child";
		}
	}

	void RenderSelfCollisionModeOption(
		const char* Label,
		ERagdollSelfCollisionMode Mode,
		USkeletalMeshDebugComponent* MeshComponent)
	{
		if (ImGui::Selectable(Label, SelfCollisionMode == Mode))
		{
			SelfCollisionMode = Mode;
			if (MeshComponent)
			{
				MeshComponent->SetRagdollSelfCollisionMode(SelfCollisionMode);
			}
		}
	}

private:
	bool bSimulationActive = false;
	bool bPaused = false;
	bool bGravityEnabled = true;
	bool bCreateConstraints = true;
	bool bIncludeSelectedSelf = true;
	float BlendWeight = 1.0f;
	ERagdollSelfCollisionMode SelfCollisionMode = ERagdollSelfCollisionMode::DisableParentChild;
	TArray<FTransform> InitialLocalPose;
	FTransform InitialRelativeTransform;
	bool bHasInitialRelativeTransform = false;
};

FMeshEditorSkeletonTab::FMeshEditorSkeletonTab(FMeshEditorWidget& InOwner)
	: FMeshEditorWidgetTab(InOwner)
{
}

bool FMeshEditorSkeletonTab::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<USkeletalMesh>();
}

bool FMeshEditorSkeletonTab::IsEditingObject(UObject* Object) const
{
	return IsEditingCurrentSkeletalMesh(Object);
}

bool FMeshEditorSkeletonTab::ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const
{
	if (!Object || !Object->IsA<USkeletalMesh>())
	{
		return false;
	}

	OutObjectToEdit = Object;
	OutInitialTab = EMeshEditorTab::Skeleton;
	return true;
}

void FMeshEditorSkeletonTab::Reset()
{
	SelectedBoneIndex = -1;
}

void FMeshEditorSkeletonTab::OnEditorOpened()
{
	GetViewportClient().CreateBoneDebugComponent();
	GetViewportClient().SetSelectedBone(GetSkeletalMesh(), -1);
}

void FMeshEditorSkeletonTab::OnActivated(EMeshEditorTab PreviousTab)
{
	(void)PreviousTab;
	if (USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent())
	{
		Comp->ApplyBoneEditBasePose();
	}
	GetViewportClient().GetRenderOptions().WeightBoneHeatMapBoneIndex = SelectedBoneIndex;
}

void FMeshEditorSkeletonTab::Render(float AvailableHeight)
{
	(void)AvailableHeight;
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();

	ImGui::BeginChild("BoneHierarchy", ImVec2(HierarchyWidth, 0), true);
	ImGui::Text("Bone Hierarchy");
	ImGui::Separator();
	if (SkeletalMesh)
	{
		const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		if (Asset)
		{
			for (int32 i = 0; i < static_cast<int32>(Asset->Bones.size()); ++i)
			{
				if (Asset->Bones[i].ParentIndex == -1)
				{
					RenderBoneTree(Asset, i);
				}
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::Button("##skelSplitter", ImVec2(4.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		HierarchyWidth = std::max(100.0f, std::min(HierarchyWidth, ImGui::GetWindowWidth() - DetailsWidth - 100.0f));
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	ImGui::SameLine();

	ImGui::BeginGroup();
	{
		float ViewportWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("BoneDetails", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Bone Details");
	ImGui::Separator();

	if (SkeletalMesh && SelectedBoneIndex != -1)
	{
		FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		FBone& Bone = Asset->Bones[SelectedBoneIndex];

		ImGui::Text("Name: %s", Bone.Name.c_str());
		ImGui::Text("Index: %d", SelectedBoneIndex);
		ImGui::Dummy(ImVec2(0, 10));

		USkeletalMeshComponent* PreviewMeshComponent = GetViewportClient().GetPreviewMeshComponent();
		FTransform LocalTransform = PreviewMeshComponent
			? PreviewMeshComponent->GetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex)
			: FTransform(Bone.GetReferenceLocalPose());

		FVector Location = LocalTransform.Location;
		if (ImGui::DragFloat3("Location", &Location.X, 0.1f))
		{
			LocalTransform.Location = Location;
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
		}

		FVector Rotation = LocalTransform.GetRotator().ToVector();
		if (ImGui::DragFloat3("Rotation", &Rotation.X, 0.1f))
		{
			LocalTransform.SetRotation(FRotator(Rotation));
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
		}

		FVector Scale = LocalTransform.Scale;
		if (ImGui::DragFloat3("Scale", &Scale.X, 0.1f, 0.01f))
		{
			LocalTransform.Scale = Scale;
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
		}
	}
	else
	{
		ImGui::TextDisabled("Select a bone to edit.");
	}

	ImGui::EndChild();
}

void FMeshEditorSkeletonTab::RenderBoneTree(const FSkeletalMesh* Asset, int32 Index)
{
	if (!Asset || Index < 0 || Index >= static_cast<int32>(Asset->Bones.size()))
	{
		return;
	}

	const FBone& Bone = Asset->Bones[Index];
	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;
	if (Index == SelectedBoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bHasChildren = false;
	for (int32 i = Index + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
	{
		if (Asset->Bones[i].ParentIndex == Index)
		{
			bHasChildren = true;
			break;
		}
	}
	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	bool bOpen = ImGui::TreeNodeEx(Bone.Name.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SelectedBoneIndex = Index;
		GetViewportClient().SetSelectedBone(GetSkeletalMesh(), Index);
		GetViewportClient().GetRenderOptions().WeightBoneHeatMapBoneIndex = SelectedBoneIndex;
	}

	if (bOpen && bHasChildren)
	{
		for (int32 i = Index + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
		{
			if (Asset->Bones[i].ParentIndex == Index)
			{
				RenderBoneTree(Asset, i);
			}
		}
		ImGui::TreePop();
	}
}

FMeshEditorMeshTab::FMeshEditorMeshTab(FMeshEditorWidget& InOwner)
	: FMeshEditorWidgetTab(InOwner)
{
}

bool FMeshEditorMeshTab::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<USkeletalMesh>();
}

bool FMeshEditorMeshTab::IsEditingObject(UObject* Object) const
{
	return IsEditingCurrentSkeletalMesh(Object);
}

void FMeshEditorMeshTab::Render(float AvailableHeight)
{
	(void)AvailableHeight;
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();

	const float StatsWidth = 220.0f;
	ImGui::BeginChild("MeshInfo", ImVec2(StatsWidth, 0), true);
	ImGui::Text("Mesh Info");
	ImGui::Separator();
	if (SkeletalMesh)
	{
		const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		if (Asset)
		{
			ImGui::Text("Vertices:  %s", FormatMeshStatCount(Asset->Vertices.size()).c_str());
			ImGui::Text("Triangles: %s", FormatMeshStatCount(Asset->Indices.size() / 3).c_str());
			ImGui::Text("Bones:     %zu", Asset->Bones.size());
			ImGui::Text("Morphs:    %zu", Asset->MorphTargets.size());
			USkeletalMeshComponent* PreviewMeshComponent = GetViewportClient().GetPreviewMeshComponent();
			if (!Asset->MorphTargets.empty() && PreviewMeshComponent)
			{
				ImGui::Dummy(ImVec2(0, 8));
				ImGui::Separator();
				ImGui::TextUnformatted("Morph Preview");
				if (ImGui::SmallButton("Reset Morphs"))
				{
					PreviewMeshComponent->ClearMorphTargetWeights();
				}
				for (int32 MorphIndex = 0; MorphIndex < static_cast<int32>(Asset->MorphTargets.size()); ++MorphIndex)
				{
					const FMorphTarget& MorphTarget = Asset->MorphTargets[MorphIndex];
					float Weight = PreviewMeshComponent->GetMorphTargetWeightByIndex(MorphIndex);
					ImGui::PushID(MorphIndex);
					const char* Label = MorphTarget.Name.empty() ? "Unnamed" : MorphTarget.Name.c_str();
					if (ImGui::SliderFloat(Label, &Weight, -1.0f, 1.0f, "%.3f"))
					{
						PreviewMeshComponent->SetMorphTargetWeightByIndex(MorphIndex, Weight);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%zu vertex deltas", MorphTarget.Deltas.size());
					}
					ImGui::PopID();
				}
			}
			ImGui::Dummy(ImVec2(0, 8));
			const FString& Path = SkeletalMesh->GetAssetPathFileName();
			if (!Path.empty() && Path != "None")
			{
				ImGui::TextWrapped("Path:\n%s", Path.c_str());
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginGroup();
	{
		ImVec2 Size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();
}

FMeshEditorAnimationTab::FMeshEditorAnimationTab(FMeshEditorWidget& InOwner)
	: FMeshEditorWidgetTab(InOwner)
{
}

bool FMeshEditorAnimationTab::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<USkeletalMesh>();
}

bool FMeshEditorAnimationTab::IsEditingObject(UObject* Object) const
{
	return IsEditingCurrentSkeletalMesh(Object);
}

void FMeshEditorAnimationTab::Reset()
{
	AnimTabState = FAnimationTabState {};
}

void FMeshEditorAnimationTab::OnEditorOpened()
{
	FAnimationManager::Get().RefreshAvailableAnimations();
	MarkAnimationListDirty();
}

void FMeshEditorAnimationTab::Tick(float DeltaTime)
{
	USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent();
	if (!Comp)
	{
		return;
	}
	UAnimSingleNodeInstance* NodeInst = Comp->GetAnimNodeInstance(FName::None);
	if (!NodeInst)
	{
		return;
	}

	NodeInst->UpdateAnimation(DeltaTime);

	USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
	if (!Mesh)
	{
		return;
	}
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty())
	{
		return;
	}

	FPoseContext Out;
	Out.SkeletalMesh = Mesh;
	Out.Pose.resize(Asset->Bones.size());
	Out.ResetToRefPose();

	NodeInst->EvaluatePose(Out);
	ApplyMorphPreviewOverrides(Out.MorphWeights);

	Comp->SetAnimationPose(Out.Pose, Out.MorphWeights);
}

void FMeshEditorAnimationTab::ApplyAnimationToComponent()
{
	USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent();
	if (!Comp || !AnimTabState.CurrentSequence)
	{
		return;
	}
	Comp->PlayAnimation(AnimTabState.CurrentSequence, true);
	Comp->SetPlaying(false);
	Comp->SetPlayRate(1.0f);
	ResetMorphPreviewOverrides();
}

void FMeshEditorAnimationTab::EnsureMorphPreviewOverrideSize()
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	const size_t MorphCount = MeshAsset ? MeshAsset->MorphTargets.size() : 0;
	if (AnimTabState.MorphPreviewWeights.size() != MorphCount)
	{
		AnimTabState.MorphPreviewWeights.assign(MorphCount, 0.0f);
	}
	if (AnimTabState.MorphPreviewOverrideMask.size() != MorphCount)
	{
		AnimTabState.MorphPreviewOverrideMask.assign(MorphCount, 0);
	}
}

void FMeshEditorAnimationTab::ResetMorphPreviewOverrides()
{
	AnimTabState.MorphPreviewWeights.clear();
	AnimTabState.MorphPreviewOverrideMask.clear();
	AnimTabState.bMorphPreviewOverrideEnabled = false;
}

void FMeshEditorAnimationTab::ApplyMorphPreviewOverrides(TArray<float>& InOutMorphWeights) const
{
	if (!AnimTabState.bMorphPreviewOverrideEnabled)
	{
		return;
	}
	const size_t Count = AnimTabState.MorphPreviewWeights.size();
	if (Count == 0 || AnimTabState.MorphPreviewOverrideMask.size() != Count)
	{
		return;
	}
	if (InOutMorphWeights.size() < Count)
	{
		InOutMorphWeights.resize(Count, 0.0f);
	}
	for (size_t Index = 0; Index < Count; ++Index)
	{
		if (AnimTabState.MorphPreviewOverrideMask[Index] != 0)
		{
			InOutMorphWeights[Index] = AnimTabState.MorphPreviewWeights[Index];
		}
	}
}

void FMeshEditorAnimationTab::RefreshAnimationPreviewPose()
{
	USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent();
	if (!Comp)
	{
		return;
	}
	UAnimSingleNodeInstance* NodeInst = Comp->GetAnimNodeInstance(FName::None);
	if (!NodeInst)
	{
		return;
	}
	USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
	if (!Mesh)
	{
		return;
	}
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty())
	{
		return;
	}

	FPoseContext Out;
	Out.SkeletalMesh = Mesh;
	Out.Pose.resize(Asset->Bones.size());
	Out.ResetToRefPose();
	NodeInst->EvaluatePose(Out);
	ApplyMorphPreviewOverrides(Out.MorphWeights);
	Comp->SetAnimationPose(Out.Pose, Out.MorphWeights);
}

void FMeshEditorAnimationTab::MarkAnimationListDirty()
{
	AnimTabState.bAnimationListDirty = true;
}

const TArray<FAssetListItem>& FMeshEditorAnimationTab::GetCachedAnimationFilesForCurrentSkeleton()
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	FSkeletonBinding CurrentBinding;

	if (SkeletalMesh)
	{
		CurrentBinding = SkeletalMesh->GetSkeletonBinding();
	}
	else
	{
		CurrentBinding.Reset();
	}

	if (AnimTabState.bAnimationListDirty ||
		!IsSameSkeletonBindingForAnimationList(AnimTabState.CachedAnimationListBinding, CurrentBinding))
	{
		AnimTabState.CachedAnimationFiles.clear();
		AnimTabState.CachedMontageFiles.clear();
		AnimTabState.CachedAnimationListBinding = CurrentBinding;

		if (SkeletalMesh)
		{
			AnimTabState.CachedAnimationFiles = FAssetRegistry::ListAnimationsForSkeleton(CurrentBinding, false);
			AnimTabState.CachedMontageFiles = FAssetRegistry::ListMontagesForSkeleton(CurrentBinding, false);
		}

		AnimTabState.bAnimationListDirty = false;
	}

	return AnimTabState.CachedAnimationFiles;
}

const TArray<FAssetListItem>& FMeshEditorAnimationTab::GetCachedMontageFilesForCurrentSkeleton()
{
	GetCachedAnimationFilesForCurrentSkeleton();
	return AnimTabState.CachedMontageFiles;
}

void FMeshEditorAnimationTab::Render(float AvailableHeight)
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();

	constexpr float TimelineHeight = 210.0f;
	const float ContentHeight = AvailableHeight - TimelineHeight - ImGui::GetStyle().ItemSpacing.y * 3.0f;

	ImGui::BeginChild("AssetDetails", ImVec2(AnimTabState.AnimDetailsWidth, ContentHeight), true);
	if (AnimTabState.bMontageSelected && AnimTabState.CurrentMontage)
	{
		USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent();
		UAnimInstance* AnimInst = Comp ? Comp->GetAnimInstance() : nullptr;
		FAnimMontagePropertyPanel::Render(AnimTabState.CurrentMontage, Comp, AnimInst);
	}
	else if (AnimTabState.CurrentSequence)
	{
		UAnimSequence* Seq = AnimTabState.CurrentSequence;
		const int32 NotifyCount = static_cast<int32>(Seq->GetNotifies().size());
		const bool bShowNotifyDetails =
			AnimTabState.SelectedNotifyIndex >= 0 &&
			AnimTabState.SelectedNotifyIndex < NotifyCount;
		const bool bShowMorphDetails =
			AnimTabState.SelectedMorphCurveIndex >= 0 &&
			AnimTabState.SelectedMorphCurveIndex < static_cast<int32>(Seq->GetMorphTargetCurves().size());

		if (bShowNotifyDetails)
		{
			FAnimationTimelinePanel::RenderNotifyDetails(Seq, AnimTabState.SelectedNotifyIndex);
		}
		else if (bShowMorphDetails)
		{
			if (FAnimationTimelinePanel::RenderMorphDetails(
				Seq,
				SkeletalMesh,
				AnimTabState.SelectedMorphCurveIndex,
				AnimTabState.SelectedMorphKeyIndex))
			{
				RefreshAnimationPreviewPose();
			}
		}
		else
		{
			ImGui::TextUnformatted("Asset Details");
			ImGui::Separator();
			ImGui::Text("Name:   %s", Seq->GetName().c_str());
			ImGui::Text("Length: %.3f s", Seq->GetPlayLength());
			ImGui::Text("FPS:    %.1f", Seq->GetFrameRate());
			ImGui::Text("Frames: %d", Seq->GetNumberOfFrames());
			ImGui::Dummy(ImVec2(0, 6));
			const FString& Path = Seq->GetAssetPathFileName();
			if (!Path.empty() && Path != "None")
			{
				ImGui::TextWrapped("Path:\n%s", Path.c_str());
			}

			ImGui::Dummy(ImVec2(0, 12));
			FAnimSequencePropertyPanel::Render(Seq);

			USkeletalMeshComponent* PreviewMeshComponent = GetViewportClient().GetPreviewMeshComponent();
			USkeletalMesh* PreviewMesh = PreviewMeshComponent ? PreviewMeshComponent->GetSkeletalMesh() : SkeletalMesh;
			FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
			if (MeshAsset && !MeshAsset->MorphTargets.empty())
			{
				ImGui::Dummy(ImVec2(0, 12));
				ImGui::Separator();
				ImGui::TextUnformatted("Morph Preview / Keys");
				EnsureMorphPreviewOverrideSize();
				if (ImGui::SmallButton("Clear Morph Preview"))
				{
					ResetMorphPreviewOverrides();
					RefreshAnimationPreviewPose();
				}
				for (int32 MorphIndex = 0; MorphIndex < static_cast<int32>(MeshAsset->MorphTargets.size()); ++MorphIndex)
				{
					const FMorphTarget& MorphTarget = MeshAsset->MorphTargets[MorphIndex];
					float CurrentWeight = 0.0f;
					if (MorphIndex < static_cast<int32>(AnimTabState.MorphPreviewWeights.size()) &&
						AnimTabState.MorphPreviewOverrideMask[MorphIndex] != 0)
					{
						CurrentWeight = AnimTabState.MorphPreviewWeights[MorphIndex];
					}
					else if (PreviewMeshComponent)
					{
						CurrentWeight = PreviewMeshComponent->GetMorphTargetWeightByIndex(MorphIndex);
					}

					ImGui::PushID(MorphIndex);
					const char* Label = MorphTarget.Name.empty() ? "Unnamed" : MorphTarget.Name.c_str();
					if (ImGui::SliderFloat(Label, &CurrentWeight, -1.0f, 1.0f, "%.3f"))
					{
						AnimTabState.MorphPreviewWeights[MorphIndex] = CurrentWeight;
						AnimTabState.MorphPreviewOverrideMask[MorphIndex] = 1;
						AnimTabState.bMorphPreviewOverrideEnabled = true;
						RefreshAnimationPreviewPose();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Key"))
					{
						FMorphTargetCurve& Curve = FindOrAddMorphCurve(Seq, MorphTarget.Name);
						AddOrUpdateMorphCurveKey(
							Curve,
							PreviewMeshComponent && PreviewMeshComponent->GetAnimNodeInstance(FName::None)
								? PreviewMeshComponent->GetAnimNodeInstance(FName::None)->GetCurrentTime()
								: 0.0f,
							CurrentWeight);
						AnimTabState.MorphPreviewOverrideMask[MorphIndex] = 0;
						bool bAnyOverride = false;
						for (uint8 Mask : AnimTabState.MorphPreviewOverrideMask)
						{
							if (Mask != 0)
							{
								bAnyOverride = true;
								break;
							}
						}
						AnimTabState.bMorphPreviewOverrideEnabled = bAnyOverride;
						FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
						RefreshAnimationPreviewPose();
					}
					ImGui::PopID();
				}
			}
		}
	}
	else
	{
		ImGui::TextUnformatted("Asset Details");
		ImGui::Separator();
		ImGui::TextDisabled("No animation selected.");
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginGroup();
	{
		float ViewportWidth = ImGui::GetContentRegionAvail().x - AnimTabState.AnimListWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(ViewportWidth, ContentHeight);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("AssetBrowser", ImVec2(AnimTabState.AnimListWidth, ContentHeight), true);
	ImGui::TextUnformatted("Asset Browser");
	ImGui::Separator();

	if (ImGui::Button("Load...", ImVec2(-1.0f, 0.0f)))
	{
		FEditorFileDialogOptions Opts;
		Opts.Filter = L"Animation Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0";
		Opts.Title = L"Load Animation";
		Opts.bReturnRelativeToProjectRoot = true;
		FString Path = FEditorFileUtils::OpenFileDialog(Opts);
		if (!Path.empty())
		{
			UAnimSequence* Seq = FAnimationManager::Get().LoadAnimation(Path);
			if (Seq && Seq->IsCompatibleWith(SkeletalMesh))
			{
				AnimTabState.CurrentSequence = Seq;
				AnimTabState.SelectedAnimIndex = -1;
				AnimTabState.SelectedNotifyIndex = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex = -1;
				ApplyAnimationToComponent();
			}
		}
	}

	if (ImGui::Button("Import Animation FBX", ImVec2(-1.0f, 0.0f)))
	{
		FEditorFileDialogOptions Opts;
		Opts.Filter = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
		Opts.Title = L"Import Animation FBX";
		Opts.bReturnRelativeToProjectRoot = true;
		FString Path = FEditorFileUtils::OpenFileDialog(Opts);
		if (!Path.empty())
		{
			FFbxImportOptionsDialog::BeginAnimationImport(AnimTabState.AnimationImportDialog, Path);
		}
	}

	if (ImGui::Button("+ New Morph Animation", ImVec2(-1.0f, 0.0f)) && SkeletalMesh)
	{
		UAnimSequence* Seq = UObjectManager::Get().CreateObject<UAnimSequence>();
		UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(Seq);
		DataModel->SetTiming(1.0f, 30.0f, 0);
		Seq->SetDataModel(DataModel);
		Seq->SetSkeletonBinding(SkeletalMesh->GetSkeletonBinding());
		Seq->SetFName(FName("MorphAnimation"));
		const FString AnimPath = FAnimationManager::GetAnimationPathForSkeleton(
			SkeletalMesh->GetAssetPathFileName(),
			"MorphAnimation",
			SkeletalMesh->GetSkeletonBinding().SkeletonPath);
		if (FAnimationManager::Get().SaveAnimation(Seq, AnimPath, SkeletalMesh->GetAssetPathFileName()))
		{
			AnimTabState.CurrentSequence = Seq;
			AnimTabState.SelectedAnimIndex = -1;
			AnimTabState.SelectedNotifyIndex = -1;
			AnimTabState.SelectedMorphCurveIndex = -1;
			AnimTabState.SelectedMorphKeyIndex = -1;
			ApplyAnimationToComponent();
			FAnimationManager::Get().RefreshAvailableAnimations();
			MarkAnimationListDirty();
		}
	}

	FAnimationImportRequest AnimationImportRequest;
	const EFbxImportDialogResult AnimationImportDialogResult = FFbxImportOptionsDialog::RenderAnimationImportPopup(
		"Import Animation FBX Options",
		AnimTabState.AnimationImportDialog,
		SkeletalMesh ? SkeletalMesh->GetSkeletonBinding().SkeletonPath : FString("None"),
		AnimationImportRequest);

	if (AnimationImportDialogResult == EFbxImportDialogResult::Submitted)
	{
		TArray<UAnimSequence*> ImportedSequences;
		FAnimationManager::Get().ImportAnimationForSkeleton(AnimationImportRequest, &ImportedSequences);
		FAnimationManager::Get().RefreshAvailableAnimations();
		MarkAnimationListDirty();
		if (!ImportedSequences.empty())
		{
			AnimTabState.CurrentSequence = ImportedSequences[0];
			AnimTabState.SelectedAnimIndex = -1;
			AnimTabState.SelectedNotifyIndex = -1;
			AnimTabState.SelectedMorphCurveIndex = -1;
			AnimTabState.SelectedMorphKeyIndex = -1;
			ApplyAnimationToComponent();
			FFbxImportOptionsDialog::RequestClose(AnimTabState.AnimationImportDialog);
		}
		else
		{
			AnimTabState.AnimationImportDialog.Error =
				"No animation was imported. Existing assets may have been skipped.";
		}
	}

	ImGui::Separator();

	if (ImGui::SmallButton("Refresh Animation List"))
	{
		FAnimationManager::Get().RefreshAvailableAnimations();
		FAnimationManager::Get().RefreshAvailableMontages();
		MarkAnimationListDirty();
	}

	if (!AnimTabState.bMontagesScanned)
	{
		FAnimationManager::Get().RefreshAvailableMontages();
		AnimTabState.bMontagesScanned = true;
	}

	const TArray<FAssetListItem>& AnimFiles = GetCachedAnimationFilesForCurrentSkeleton();
	const TArray<FAssetListItem>& MontageFiles = GetCachedMontageFilesForCurrentSkeleton();

	const bool bCanCreateMontage = (AnimTabState.CurrentSequence != nullptr) && !AnimTabState.bMontageSelected;
	if (!bCanCreateMontage)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("+ New Montage (from selected sequence)", ImVec2(-1.0f, 0.0f)))
	{
		const FString Stem = ExtractStem(AnimTabState.CurrentSequence->GetAssetPathFileName());
		const FString MontageName = Stem + "_Montage";
		const FString PackagePath = FString("Content/Montages/") + MontageName + ".uasset";
		UAnimMontage* Montage = FAnimationManager::Get().CreateMontage(AnimTabState.CurrentSequence, MontageName);
		if (Montage)
		{
			FAnimationManager::Get().SaveMontage(Montage, PackagePath);
			FAnimationManager::Get().RefreshAvailableMontages();
			MarkAnimationListDirty();
			AnimTabState.CurrentMontage = Montage;
			AnimTabState.bMontageSelected = true;

			const TArray<FAssetListItem>& Updated = GetCachedMontageFilesForCurrentSkeleton();
			AnimTabState.SelectedMontageIndex = -1;
			for (int32 j = 0; j < static_cast<int32>(Updated.size()); ++j)
			{
				if (Updated[j].FullPath == PackagePath)
				{
					AnimTabState.SelectedMontageIndex = j;
					break;
				}
			}
		}
	}
	if (!bCanCreateMontage)
	{
		ImGui::EndDisabled();
	}

	struct FEntry
	{
		FString DisplayName;
		FString FullPath;
		bool bIsMontage = false;
		int32 OriginalIndex = -1;
	};
	TArray<FEntry> Entries;
	Entries.reserve(AnimFiles.size() + MontageFiles.size());
	for (int32 i = 0; i < static_cast<int32>(AnimFiles.size()); ++i)
	{
		Entries.push_back({ AnimFiles[i].DisplayName, AnimFiles[i].FullPath, false, i });
	}
	for (int32 i = 0; i < static_cast<int32>(MontageFiles.size()); ++i)
	{
		Entries.push_back({ MontageFiles[i].DisplayName, MontageFiles[i].FullPath, true, i });
	}
	std::sort(Entries.begin(), Entries.end(),
		[](const FEntry& A, const FEntry& B) { return A.DisplayName < B.DisplayName; });

	ImGui::TextUnformatted("Animations & Montages");
	for (const FEntry& E : Entries)
	{
		const bool bSelected =
			E.bIsMontage
				? (AnimTabState.bMontageSelected && AnimTabState.SelectedMontageIndex == E.OriginalIndex)
				: (!AnimTabState.bMontageSelected && AnimTabState.SelectedAnimIndex == E.OriginalIndex);

		const ImU32 Color = E.bIsMontage ? IM_COL32(255, 200, 100, 255) : IM_COL32(255, 255, 255, 255);
		ImGui::PushStyleColor(ImGuiCol_Text, Color);

		const FString Label = (E.bIsMontage ? "[M] " : "      ") + E.DisplayName;
		if (ImGui::Selectable(Label.c_str(), bSelected))
		{
			if (E.bIsMontage)
			{
				AnimTabState.SelectedMontageIndex = E.OriginalIndex;
				AnimTabState.bMontageSelected = true;
				AnimTabState.SelectedNotifyIndex = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex = -1;
				ResetMorphPreviewOverrides();
				if (UAnimMontage* M = FAnimationManager::Get().LoadMontage(E.FullPath))
				{
					AnimTabState.CurrentMontage = M;
				}
			}
			else
			{
				AnimTabState.SelectedAnimIndex = E.OriginalIndex;
				AnimTabState.bMontageSelected = false;
				AnimTabState.SelectedNotifyIndex = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex = -1;
				if (UAnimSequence* Seq = FAnimationManager::Get().LoadAnimation(E.FullPath))
				{
					if (Seq->IsCompatibleWith(SkeletalMesh))
					{
						AnimTabState.CurrentSequence = Seq;
						ApplyAnimationToComponent();
					}
				}
			}
		}
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\n%s", E.bIsMontage ? "Montage" : "Sequence", E.FullPath.c_str());
		}
	}
	ImGui::EndChild();

	UAnimSingleNodeInstance* NodeInst = nullptr;
	USkeletalMeshComponent* Comp = GetViewportClient().GetPreviewMeshComponent();
	if (Comp && AnimTabState.CurrentSequence)
	{
		NodeInst = Comp->GetAnimNodeInstance(FName::None);
	}

	if (Comp && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		!ImGui::GetIO().WantTextInput &&
		ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		const bool bPlaying = NodeInst && NodeInst->IsPlaying();
		Comp->SetPlaying(!bPlaying);
	}

	FAnimationTimelinePanel::Render(
		NodeInst,
		Comp,
		AnimTabState.CurrentSequence,
		TimelineHeight,
		AnimTabState.SelectedNotifyIndex,
		AnimTabState.SelectedMorphCurveIndex,
		AnimTabState.SelectedMorphKeyIndex);
}

FMeshEditorPhysicsAssetTab::FMeshEditorPhysicsAssetTab(FMeshEditorWidget& InOwner)
	: FMeshEditorWidgetTab(InOwner)
	, RagdollPanel(std::make_unique<FPhysicsAssetRagdollPanel>())
{
}

FMeshEditorPhysicsAssetTab::~FMeshEditorPhysicsAssetTab() = default;

bool FMeshEditorPhysicsAssetTab::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UPhysicsAsset>();
}

bool FMeshEditorPhysicsAssetTab::IsEditingObject(UObject* Object) const
{
	const USkeletalMesh* CurrentMesh = GetSkeletalMesh();
	const UPhysicsAsset* RequestedPhysicsAsset = Cast<UPhysicsAsset>(Object);
	if (!CurrentMesh || !RequestedPhysicsAsset)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	const FString RequestedPath = GetPhysicsAssetSourceMeshPath(RequestedPhysicsAsset);
	return IsValidAssetPath(CurrentPath) && CurrentPath == RequestedPath;
}

bool FMeshEditorPhysicsAssetTab::ShouldActivateOnReuse(UObject* Object) const
{
	return IsEditingObject(Object);
}

bool FMeshEditorPhysicsAssetTab::ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const
{
	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(Object);
	if (!PhysicsAsset)
	{
		return false;
	}

	USkeletalMesh* SourceMesh = ResolveSourceMeshForPhysicsAsset(PhysicsAsset);
	if (!SourceMesh)
	{
		UE_LOG("PhysicsAsset editor open failed: source skeletal mesh not found. PhysicsAsset=%s SourceMesh=%s",
			PhysicsAsset->GetAssetPathFileName().c_str(),
			PhysicsAsset->GetSourceSkeletalMeshPath().c_str());
		return false;
	}

	OutObjectToEdit = SourceMesh;
	OutInitialTab = EMeshEditorTab::PhysicsAsset;
	return true;
}

void FMeshEditorPhysicsAssetTab::Reset()
{
	if (RagdollPanel)
	{
		RagdollPanel->Reset();
	}
	ClearReflectionDetailTarget();
	SelectedPhysicsBodyIndex = -1;
	SelectedPhysicsConstraintIndex = -1;
	PendingPhysicsAssetBuildOptions = FPhysicsAssetBuildOptions {};
}

void FMeshEditorPhysicsAssetTab::Tick(float DeltaTime)
{
	if (RagdollPanel)
	{
		RagdollPanel->Tick(DeltaTime, GetViewportClient().GetPreviewDebugMeshComponent());
	}
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		DetectReflectionDetailChanges(SkeletalMesh->GetPhysicsAsset());
	}
}

void FMeshEditorPhysicsAssetTab::OnEditorOpened()
{
	GetViewportClient().CreatePhysicsAssetDebugComponent();
	GetViewportClient().SetOnPhysicsAssetBodyPicked([this](int32 BodyIndex)
	{
		OnPhysicsAssetBodyPicked(BodyIndex);
	});
	GetViewportClient().SetOnPhysicsAssetConstraintPicked([this](int32 ConstraintIndex)
	{
		OnPhysicsAssetConstraintPicked(ConstraintIndex);
	});
	GetViewportClient().SetOnPhysicsAssetShapeEdited([this]()
	{
		OnPhysicsAssetShapeEdited();
	});
	GetViewportClient().SetOnPhysicsAssetConstraintEdited([this]()
	{
		OnPhysicsAssetConstraintEdited();
	});
}

void FMeshEditorPhysicsAssetTab::OnEditorClosing()
{
	if (RagdollPanel)
	{
		RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
	}
	ClearReflectionDetailTarget();
	GetViewportClient().SetPhysicsAssetPickingEnabled(false);
	GetViewportClient().SetOnPhysicsAssetBodyPicked(nullptr);
	GetViewportClient().SetOnPhysicsAssetConstraintPicked(nullptr);
	GetViewportClient().SetOnPhysicsAssetShapeEdited(nullptr);
	GetViewportClient().SetOnPhysicsAssetConstraintEdited(nullptr);
}

void FMeshEditorPhysicsAssetTab::OnActivated(EMeshEditorTab PreviousTab)
{
	(void)PreviousTab;
	GetViewportClient().SetPhysicsAssetPickingEnabled(true);
	GetViewportClient().GetRenderOptions().ShowFlags.bDebugPhysicsAsset = true;
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		SyncReflectionDetailTarget(SkeletalMesh->GetPhysicsAsset());
	}
}

void FMeshEditorPhysicsAssetTab::OnDeactivated(EMeshEditorTab NextTab)
{
	(void)NextTab;
	if (RagdollPanel)
	{
		RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
	}
	ClearReflectionDetailTarget();
	GetViewportClient().SetPhysicsAssetPickingEnabled(false);
}

void FMeshEditorPhysicsAssetTab::Render(float AvailableHeight)
{
	(void)AvailableHeight;
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	UPhysicsAsset* PhysicsAsset = SkeletalMesh ? SkeletalMesh->GetPhysicsAsset() : nullptr;
	GetViewportClient().GetRenderOptions().ShowFlags.bDebugPhysicsAsset = true;
	SyncDebugComponent(PhysicsAsset);

	constexpr float BodyListWidth = 260.0f;

	ImGui::BeginChild("PhysicsAssetBodies", ImVec2(BodyListWidth, 0), true);
	ImGui::TextUnformatted("Physics Asset");
	ImGui::Separator();

	if (!SkeletalMesh)
	{
		ImGui::TextDisabled("No skeletal mesh.");
		ImGui::EndChild();
		ClearReflectionDetailTarget();
		return;
	}

	if (PhysicsAsset)
	{
		ImGui::Text("Bodies: %zu", PhysicsAsset->GetBodySetups().size());
	}
	else
	{
		ImGui::TextDisabled("None");
	}

	ImGui::Separator();
	if (RagdollPanel)
	{
		RagdollPanel->Render(
			GetViewportClient().GetPreviewDebugMeshComponent(),
			PhysicsAsset,
			SelectedPhysicsBodyIndex);
		ImGui::Separator();
	}
	RenderPhysicsAssetBodyList(SkeletalMesh, PhysicsAsset);
	ImGui::EndChild();
	SyncReflectionDetailTarget(PhysicsAsset);
	DetectReflectionDetailChanges(PhysicsAsset);

	ImGui::SameLine();

	ImGui::BeginGroup();
	const float ViewportWidth = std::max(
		120.0f,
		ImGui::GetContentRegionAvail().x);
	const ImVec2 ViewportSize = ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y);
	const ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	RenderViewportPanel(ViewportSize);
	if (RenderPhysicsAssetBuildOptionsOverlay(ViewportPos, ViewportSize, SkeletalMesh, PhysicsAsset))
	{
		FSlateApplication::Get().SetViewportImGuiHovered(&GetViewportClient(), false);
	}
	ImGui::EndGroup();
}

bool FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBuildOptionsOverlay(
	const ImVec2& ViewportPos,
	const ImVec2& ViewportSize,
	USkeletalMesh* SkeletalMesh,
	UPhysicsAsset*& InOutPhysicsAsset)
{
	constexpr float OverlayWidth = 300.0f;
	constexpr float Padding = 8.0f;
	const float ClampedWidth = std::max(1.0f, std::min(OverlayWidth, ViewportSize.x - Padding * 2.0f));
	const ImVec2 OverlayPos(ViewportPos.x + ViewportSize.x - Padding, ViewportPos.y + ViewportSize.y - Padding);
	const FString WindowId = "Physics Asset Build Options##PhysicsAssetBuildOptionsOverlay_" + std::to_string(GetOwnerInstanceId());

	ImGui::SetNextWindowPos(OverlayPos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(ClampedWidth, 0.0f), ImGuiCond_Always);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 24, 28, 225));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 50));

	const ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize;

	bool bHovered = false;
	if (!ImGui::Begin(WindowId.c_str(), nullptr, Flags))
	{
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
		return false;
	}

	bHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	ImGui::PushID(WindowId.c_str());

	ImGui::TextUnformatted("Physics Asset Build Options");
	ImGui::Separator();

	const auto GetGeomTypeLabel = [](EPhysicsAssetFitGeomType GeomType) -> const char*
	{
		switch (GeomType)
		{
		case EPhysicsAssetFitGeomType::Box:
			return "Box";
		case EPhysicsAssetFitGeomType::Sphere:
			return "Sphere";
		case EPhysicsAssetFitGeomType::Sphyl:
		default:
			return "Capsule";
		}
	};

	const auto BeginOptionRow = [](const char* Label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-1.0f);
	};
	const auto EndOptionRow = []()
	{
		ImGui::PopItemWidth();
	};

	if (ImGui::BeginTable("##PhysicsAssetBuildOptionTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 142.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		BeginOptionRow("Use Dominant Bone Weight");
		ImGui::Checkbox("##UseDominantBoneWeight", &PendingPhysicsAssetBuildOptions.bUseDominantBoneWeight);
		EndOptionRow();

		BeginOptionRow("Auto Orient To Bone");
		ImGui::Checkbox("##AutoOrientToBone", &PendingPhysicsAssetBuildOptions.bAutoOrientToBone);
		EndOptionRow();

		BeginOptionRow("Walk Past Small Bones");
		ImGui::Checkbox("##WalkPastSmallBones", &PendingPhysicsAssetBuildOptions.bWalkPastSmall);
		EndOptionRow();

		BeginOptionRow("Create Body For All Bones");
		ImGui::Checkbox("##CreateBodyForAllBones", &PendingPhysicsAssetBuildOptions.bBodyForAll);
		EndOptionRow();

		BeginOptionRow("Primitive Type");
		if (ImGui::BeginCombo("##PrimitiveType", GetGeomTypeLabel(PendingPhysicsAssetBuildOptions.GeomType)))
		{
			if (ImGui::Selectable("Capsule", PendingPhysicsAssetBuildOptions.GeomType == EPhysicsAssetFitGeomType::Sphyl))
			{
				PendingPhysicsAssetBuildOptions.GeomType = EPhysicsAssetFitGeomType::Sphyl;
			}
			if (ImGui::Selectable("Box", PendingPhysicsAssetBuildOptions.GeomType == EPhysicsAssetFitGeomType::Box))
			{
				PendingPhysicsAssetBuildOptions.GeomType = EPhysicsAssetFitGeomType::Box;
			}
			if (ImGui::Selectable("Sphere", PendingPhysicsAssetBuildOptions.GeomType == EPhysicsAssetFitGeomType::Sphere))
			{
				PendingPhysicsAssetBuildOptions.GeomType = EPhysicsAssetFitGeomType::Sphere;
			}
			ImGui::EndCombo();
		}
		EndOptionRow();

		BeginOptionRow("Min Bone Size");
		ImGui::DragFloat("##MinBoneSize", &PendingPhysicsAssetBuildOptions.MinBoneSize, 0.25f, 0.0f, 1000.0f, "%.2f");
		EndOptionRow();

		BeginOptionRow("Min Weld Size");
		ImGui::DragFloat("##MinWeldSize", &PendingPhysicsAssetBuildOptions.MinWeldSize, 0.0001f, 0.0f, 1000.0f, "%.4f");
		EndOptionRow();

		BeginOptionRow("Fit Padding");
		ImGui::DragFloat("##FitPadding", &PendingPhysicsAssetBuildOptions.FitPadding, 0.001f, 1.0f, 2.0f, "%.3f");
		EndOptionRow();

		BeginOptionRow("Min Primitive Size");
		ImGui::DragFloat("##MinPrimitiveSize", &PendingPhysicsAssetBuildOptions.MinPrimitiveSize, 0.01f, 0.01f, 1000.0f, "%.2f");
		EndOptionRow();

		ImGui::EndTable();
	}

	ImGui::Separator();

	if (!SkeletalMesh)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Generate", ImVec2(-1.0f, 0.0f)))
	{
		if (RagdollPanel)
		{
			RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
		}
		PendingPhysicsAssetBuildOptions.MinBoneSize = std::max(0.0f, PendingPhysicsAssetBuildOptions.MinBoneSize);
		PendingPhysicsAssetBuildOptions.MinWeldSize = std::max(0.0f, PendingPhysicsAssetBuildOptions.MinWeldSize);
		PendingPhysicsAssetBuildOptions.FitPadding = std::max(1.0f, PendingPhysicsAssetBuildOptions.FitPadding);
		PendingPhysicsAssetBuildOptions.MinPrimitiveSize = std::max(0.01f, PendingPhysicsAssetBuildOptions.MinPrimitiveSize);

		UPhysicsAsset* NewAsset = FPhysicsAssetBuilder::CreateFromSkeletalMesh(SkeletalMesh, PendingPhysicsAssetBuildOptions);
		if (NewAsset)
		{
			SavePhysicsAssetChange("PhysicsAsset generate warning");
			MarkDirty();
			GetViewportClient().GetRenderOptions().ShowFlags.bDebugPhysicsAsset = true;
			InOutPhysicsAsset = NewAsset;
			SelectedPhysicsBodyIndex = -1;
			SelectedPhysicsConstraintIndex = -1;
			SyncDebugComponent(NewAsset);
			SyncReflectionDetailTarget(NewAsset);
		}
	}

	if (!SkeletalMesh)
	{
		ImGui::EndDisabled();
	}

	ImGui::PopID();
	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);
	return bHovered;
}

void FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBodyList(USkeletalMesh* SkeletalMesh, UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		ImGui::TextDisabled("No physics asset.");
		ClearReflectionDetailTarget();
		return;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (SelectedPhysicsBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		SelectedPhysicsBodyIndex = -1;
		SelectedPhysicsConstraintIndex = -1;
		RefreshReflectionDetailSnapshot(PhysicsAsset);
	}
	if (SelectedPhysicsConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintInitDescs().size()))
	{
		SelectedPhysicsConstraintIndex = -1;
		RefreshReflectionDetailSnapshot(PhysicsAsset);
	}
	SyncDebugComponent(PhysicsAsset);

	if (ImGui::Selectable("Physics Asset##PhysicsAssetRoot", SelectedPhysicsBodyIndex < 0 && SelectedPhysicsConstraintIndex < 0))
	{
		SelectedPhysicsBodyIndex = -1;
		SelectedPhysicsConstraintIndex = -1;
		SyncDebugComponent(PhysicsAsset);
		SyncReflectionDetailTarget(PhysicsAsset);
	}

	const FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		ImGui::TextDisabled("No source skeletal mesh.");
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == -1)
		{
			RenderPhysicsAssetBodyTree(Asset, PhysicsAsset, Index);
		}
	}
}

bool FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex)
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
				RenderPhysicsAssetBodyTree(Asset, PhysicsAsset, Index))
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

	if (Body && BodyIndex == SelectedPhysicsBodyIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!bHasVisibleChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	FString Label = BuildPhysicsBodyTreeLabel(Bone.Name, BodyIndex, Body);
	Label += "##PhysicsBodyBone";
	Label += std::to_string(BoneIndex);

	const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SelectedPhysicsBodyIndex = BodyIndex;
		SelectedPhysicsConstraintIndex = -1;
		SyncDebugComponent(PhysicsAsset);
		SyncReflectionDetailTarget(PhysicsAsset);
	}

	if (ImGui::BeginPopupContextItem())
	{
		TArray<UBodySetup*>& MutableBodies = PhysicsAsset->GetBodySetupsMutable();
		UBodySetup* MutableBody = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(MutableBodies.size()))
			? MutableBodies[BodyIndex]
			: nullptr;
		if (RenderAddPhysicsBodyShapeMenu(MutableBody))
		{
			if (RagdollPanel)
			{
				RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
			}
			SelectedPhysicsBodyIndex = BodyIndex;
			SelectedPhysicsConstraintIndex = -1;
			SavePhysicsAssetChange("PhysicsAsset body shape add warning");
			MarkDirty();
			SyncDebugComponent(PhysicsAsset);
			SyncReflectionDetailTarget(PhysicsAsset);
			RefreshReflectionDetailSnapshot(PhysicsAsset);
		}
		ImGui::EndPopup();
	}

	if (bOpen && bHasVisibleChildren)
	{
		for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
		{
			if (Asset->Bones[Index].ParentIndex == BoneIndex)
			{
				RenderPhysicsAssetBodyTree(Asset, PhysicsAsset, Index);
			}
		}
		ImGui::TreePop();
	}

	return true;
}

void FMeshEditorPhysicsAssetTab::SyncReflectionDetailTarget(UPhysicsAsset* PhysicsAsset)
{
	FSelectionDetailTarget Target;
	EPhysicsAssetDetailTargetType TargetType = EPhysicsAssetDetailTargetType::None;

	if (PhysicsAsset)
	{
		TArray<FConstraintInstanceInitDesc>& ConstraintDescs = PhysicsAsset->GetConstraintInitDescsMutable();
		if (SelectedPhysicsConstraintIndex >= 0 &&
			SelectedPhysicsConstraintIndex < static_cast<int32>(ConstraintDescs.size()))
		{
			Target.ObjectPtr = PhysicsAsset;
			Target.StructType = FConstraintInstanceInitDesc::StaticStruct();
			Target.ContainerPtr = &ConstraintDescs[SelectedPhysicsConstraintIndex];
			TargetType = EPhysicsAssetDetailTargetType::Constraint;
		}
		else
		{
			TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetupsMutable();
			if (SelectedPhysicsBodyIndex >= 0 &&
				SelectedPhysicsBodyIndex < static_cast<int32>(Bodies.size()) &&
				Bodies[SelectedPhysicsBodyIndex])
			{
				Target = FSelectionDetailTarget::FromObject(Bodies[SelectedPhysicsBodyIndex]);
				TargetType = EPhysicsAssetDetailTargetType::Body;
			}
			else
			{
				Target = FSelectionDetailTarget::FromObject(PhysicsAsset);
				TargetType = EPhysicsAssetDetailTargetType::PhysicsAsset;
			}
		}
	}

	if (!Target.IsValidTarget())
	{
		ClearReflectionDetailTarget();
		return;
	}

	const bool bTargetChanged =
		TargetType != ReflectionDetailTargetType ||
		!IsSameDetailTarget(Target, ReflectionDetailTarget);
	if (!bTargetChanged)
	{
		if (FSelectionManager* SelectionManager = GetSelectionManager())
		{
			const FSelectionDetailTarget* PrimaryTarget = SelectionManager->GetPrimaryDetailTarget();
			if (!PrimaryTarget || !IsSameDetailTarget(*PrimaryTarget, ReflectionDetailTarget))
			{
				SelectionManager->SetSingleDetailTarget(ReflectionDetailTarget);
			}
		}
		return;
	}

	ReflectionDetailTarget = Target;
	ReflectionDetailTargetType = TargetType;
	if (FSelectionManager* SelectionManager = GetSelectionManager())
	{
		SelectionManager->SetSingleDetailTarget(ReflectionDetailTarget);
	}
	RefreshReflectionDetailSnapshot(PhysicsAsset);
}

void FMeshEditorPhysicsAssetTab::ClearReflectionDetailTarget()
{
	if (FSelectionManager* SelectionManager = GetSelectionManager())
	{
		if (const FSelectionDetailTarget* PrimaryTarget = SelectionManager->GetPrimaryDetailTarget())
		{
			if (IsSameDetailTarget(*PrimaryTarget, ReflectionDetailTarget))
			{
				SelectionManager->SetSingleDetailTarget(FSelectionDetailTarget {});
			}
		}
	}

	ReflectionDetailTarget.Reset();
	ReflectionDetailTargetType = EPhysicsAssetDetailTargetType::None;
	ReflectionDetailSnapshot.clear();
	bHasReflectionDetailSnapshot = false;
	SnapshotConstraintParentLocation = FVector::ZeroVector;
	SnapshotConstraintChildLocation = FVector::ZeroVector;
}

void FMeshEditorPhysicsAssetTab::RefreshReflectionDetailSnapshot(UPhysicsAsset* PhysicsAsset)
{
	ReflectionDetailSnapshot.clear();
	bHasReflectionDetailSnapshot = false;
	SnapshotConstraintParentLocation = FVector::ZeroVector;
	SnapshotConstraintChildLocation = FVector::ZeroVector;

	switch (ReflectionDetailTargetType)
	{
	case EPhysicsAssetDetailTargetType::PhysicsAsset:
		ReflectionDetailSnapshot = CaptureObjectSnapshot(PhysicsAsset);
		break;
	case EPhysicsAssetDetailTargetType::Body:
		ReflectionDetailSnapshot = CaptureObjectSnapshot(ReflectionDetailTarget.ObjectPtr);
		break;
	case EPhysicsAssetDetailTargetType::Constraint:
	{
		const FConstraintInstanceInitDesc* ConstraintDesc =
			static_cast<const FConstraintInstanceInitDesc*>(ReflectionDetailTarget.ContainerPtr);
		ReflectionDetailSnapshot = CaptureConstraintSnapshot(ConstraintDesc);
		if (ConstraintDesc)
		{
			SnapshotConstraintParentLocation = ConstraintDesc->ParentFrame.Location;
			SnapshotConstraintChildLocation = ConstraintDesc->ChildFrame.Location;
		}
		break;
	}
	case EPhysicsAssetDetailTargetType::None:
	default:
		break;
	}

	bHasReflectionDetailSnapshot = ReflectionDetailTarget.IsValidTarget();
}

void FMeshEditorPhysicsAssetTab::DetectReflectionDetailChanges(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset || ReflectionDetailTargetType == EPhysicsAssetDetailTargetType::None)
	{
		return;
	}

	if (!ReflectionDetailTarget.IsValidTarget())
	{
		ClearReflectionDetailTarget();
		return;
	}

	if (!bHasReflectionDetailSnapshot)
	{
		RefreshReflectionDetailSnapshot(PhysicsAsset);
		return;
	}

	TArray<uint8> CurrentSnapshot;
	const char* SaveWarning = "PhysicsAsset detail edit warning";
	FConstraintInstanceInitDesc* ConstraintDesc = nullptr;

	switch (ReflectionDetailTargetType)
	{
	case EPhysicsAssetDetailTargetType::PhysicsAsset:
		CurrentSnapshot = CaptureObjectSnapshot(PhysicsAsset);
		SaveWarning = "PhysicsAsset root edit warning";
		break;
	case EPhysicsAssetDetailTargetType::Body:
		CurrentSnapshot = CaptureObjectSnapshot(ReflectionDetailTarget.ObjectPtr);
		SaveWarning = "PhysicsAsset body edit warning";
		break;
	case EPhysicsAssetDetailTargetType::Constraint:
		ConstraintDesc = static_cast<FConstraintInstanceInitDesc*>(ReflectionDetailTarget.ContainerPtr);
		CurrentSnapshot = CaptureConstraintSnapshot(ConstraintDesc);
		SaveWarning = "PhysicsAsset constraint edit warning";
		break;
	case EPhysicsAssetDetailTargetType::None:
	default:
		return;
	}

	if (CurrentSnapshot == ReflectionDetailSnapshot)
	{
		return;
	}

	if (RagdollPanel)
	{
		RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
	}

	if (ConstraintDesc)
	{
		const bool bParentChanged = HasVectorChanged(SnapshotConstraintParentLocation, ConstraintDesc->ParentFrame.Location);
		const bool bChildChanged = HasVectorChanged(SnapshotConstraintChildLocation, ConstraintDesc->ChildFrame.Location);
		if (UPhysicsAssetDebugComponent* DebugComponent = GetViewportClient().GetPhysicsAssetDebugComponent())
		{
			if (bParentChanged || bChildChanged)
			{
				DebugComponent->SyncConstraintFrameLocation(
					*ConstraintDesc,
					bParentChanged
						? EPhysicsAssetConstraintFrameSide::Parent
						: EPhysicsAssetConstraintFrameSide::Child);
			}
		}
	}

	SavePhysicsAssetChange(SaveWarning);
	MarkDirty();
	SyncDebugComponent(PhysicsAsset);
	if (UPhysicsAssetDebugComponent* DebugComponent = GetViewportClient().GetPhysicsAssetDebugComponent())
	{
		DebugComponent->MarkPhysicsAssetDebugDirty();
	}
	RefreshReflectionDetailSnapshot(PhysicsAsset);
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetBodyPicked(int32 BodyIndex)
{
	SelectedPhysicsBodyIndex = BodyIndex;
	SelectedPhysicsConstraintIndex = -1;
	UPhysicsAsset* PhysicsAsset = nullptr;
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	}
	SyncDebugComponent(PhysicsAsset);
	SyncReflectionDetailTarget(PhysicsAsset);
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetConstraintPicked(int32 ConstraintIndex)
{
	SelectedPhysicsBodyIndex = -1;
	SelectedPhysicsConstraintIndex = ConstraintIndex;
	UPhysicsAsset* PhysicsAsset = nullptr;
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	}
	SyncDebugComponent(PhysicsAsset);
	SyncReflectionDetailTarget(PhysicsAsset);
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetShapeEdited()
{
	if (RagdollPanel)
	{
		RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
	}
	SavePhysicsAssetChange("PhysicsAsset shape edit warning");
	MarkDirty();
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		RefreshReflectionDetailSnapshot(SkeletalMesh->GetPhysicsAsset());
	}
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetConstraintEdited()
{
	if (RagdollPanel)
	{
		RagdollPanel->Stop(GetViewportClient().GetPreviewDebugMeshComponent());
	}
	SavePhysicsAssetChange("PhysicsAsset constraint gizmo warning");
	MarkDirty();
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		RefreshReflectionDetailSnapshot(SkeletalMesh->GetPhysicsAsset());
	}
}

void FMeshEditorPhysicsAssetTab::SavePhysicsAssetChange(const char* LogPrefix)
{
	if (USkeletalMesh* SkeletalMesh = GetSkeletalMesh())
	{
		const FString SkeletalMeshPath = SkeletalMesh->GetAssetPathFileName();
		const bool bSavedPhysicsAsset = FPhysicsAssetManager::Get().SaveForSkeletalMesh(SkeletalMesh, SkeletalMeshPath);
		const bool bSavedSkeletalMesh = bSavedPhysicsAsset && FMeshManager::SaveSkeletalMesh(SkeletalMesh, SkeletalMeshPath);
		if (!bSavedPhysicsAsset || !bSavedSkeletalMesh)
		{
			UE_LOG("%s: failed to persist PhysicsAsset change. SkeletalMesh=%s", LogPrefix, SkeletalMeshPath.c_str());
		}
	}
}

void FMeshEditorPhysicsAssetTab::SyncDebugComponent(UPhysicsAsset* PhysicsAsset)
{
	GetViewportClient().SyncPhysicsAssetDebugComponent(
		PhysicsAsset,
		SelectedPhysicsBodyIndex,
		SelectedPhysicsConstraintIndex);
}
