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
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Editor/UI/Util/DetailPropertyRenderer.h"
#include "GameFramework/AActor.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsAssetManager.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"
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

	bool RenderConstraintInitDescDetails(
		UPhysicsAsset* PhysicsAsset,
		int32 ConstraintIndex,
		UPhysicsAssetDebugComponent* DebugComponent)
	{
		if (!PhysicsAsset || ConstraintIndex < 0)
		{
			return false;
		}

		TArray<FConstraintInstanceInitDesc>& ConstraintDescs = PhysicsAsset->GetConstraintInitDescsMutable();
		if (ConstraintIndex >= static_cast<int32>(ConstraintDescs.size()))
		{
			return false;
		}

		FConstraintInstanceInitDesc* ConstraintDesc = &ConstraintDescs[ConstraintIndex];
		ImGui::TextUnformatted("Constraint");

		const FVector PreviousParentLocation = ConstraintDesc->ParentFrame.Location;
		const FVector PreviousChildLocation = ConstraintDesc->ChildFrame.Location;
		const bool bChanged = FDetailPropertyRenderer::RenderStructProperties(
			FConstraintInstanceInitDesc::StaticStruct(),
			ConstraintDesc,
			PhysicsAsset,
			"##PhysicsAssetConstraintProps");
		if (!bChanged)
		{
			return false;
		}

		const bool bParentChanged = HasVectorChanged(PreviousParentLocation, ConstraintDesc->ParentFrame.Location);
		const bool bChildChanged = HasVectorChanged(PreviousChildLocation, ConstraintDesc->ChildFrame.Location);
		if (DebugComponent && (bParentChanged || bChildChanged))
		{
			DebugComponent->SyncConstraintFrameLocation(
				*ConstraintDesc,
				bParentChanged
					? EPhysicsAssetConstraintFrameSide::Parent
					: EPhysicsAssetConstraintFrameSide::Child);
		}

		return true;
	}

	bool RenderBodySetupDetails(UPhysicsAsset* PhysicsAsset, int32 BodyIndex)
	{
		if (!PhysicsAsset || BodyIndex < 0)
		{
			return false;
		}

		TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetupsMutable();
		if (BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[BodyIndex])
		{
			return false;
		}

		UBodySetup* BodySetup = Bodies[BodyIndex];
		ImGui::TextUnformatted("Body Setup");
		ImGui::Text("Calculated Mass: %.4f kg", BodySetup->CalculateMass());

		return FDetailPropertyRenderer::RenderStructProperties(
			UBodySetup::StaticClass(),
			BodySetup,
			PhysicsAsset,
			"##PhysicsAssetBodySetupProps");
	}

	bool RenderPhysicsAssetDetails(UPhysicsAsset* PhysicsAsset)
	{
		if (!PhysicsAsset)
		{
			return false;
		}

		ImGui::TextUnformatted("Physics Asset");
		return FDetailPropertyRenderer::RenderStructProperties(
			UPhysicsAsset::StaticClass(),
			PhysicsAsset,
			PhysicsAsset,
			"##PhysicsAssetRootProps");
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

void FMeshEditorSkeletonTab::OnPreviewActorCreated(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (USkeletalMesh* Mesh = GetSkeletalMesh())
	{
		USkeletalMeshComponent* Comp = Actor->AddComponent<USkeletalMeshComponent>();
		Comp->SetSkeletalMesh(Mesh);
		Actor->SetRootComponent(Comp);
		GetViewportClient().SetPreviewMeshComponent(Comp);
	}
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
{
}

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
	SelectedPhysicsBodyIndex = -1;
	SelectedPhysicsConstraintIndex = -1;
	bOpenPhysicsAssetBuildOptions = false;
	PendingPhysicsAssetBuildOptions = FPhysicsAssetBuildOptions {};
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
}

void FMeshEditorPhysicsAssetTab::OnDeactivated(EMeshEditorTab NextTab)
{
	(void)NextTab;
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
	constexpr float BodyDetailsWidth = 300.0f;

	ImGui::BeginChild("PhysicsAssetBodies", ImVec2(BodyListWidth, 0), true);
	ImGui::TextUnformatted("Physics Asset");
	ImGui::Separator();

	if (!SkeletalMesh)
	{
		ImGui::TextDisabled("No skeletal mesh.");
		ImGui::EndChild();
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

	if (ImGui::Button("Generate Physics Asset"))
	{
		PendingPhysicsAssetBuildOptions = FPhysicsAssetBuildOptions {};
		bOpenPhysicsAssetBuildOptions = true;
	}

	RenderPhysicsAssetBuildOptionsPopup(SkeletalMesh, PhysicsAsset);

	ImGui::Separator();
	RenderPhysicsAssetBodyList(SkeletalMesh, PhysicsAsset);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginGroup();
	const float ViewportWidth = std::max(
		120.0f,
		ImGui::GetContentRegionAvail().x - BodyDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
	const ImVec2 ViewportSize = ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y);
	RenderViewportPanel(ViewportSize);
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("PhysicsAssetBodyDetails", ImVec2(BodyDetailsWidth, 0), true);
	RenderPhysicsAssetBodyDetails(PhysicsAsset);
	ImGui::EndChild();
}

void FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBuildOptionsPopup(
	USkeletalMesh* SkeletalMesh,
	UPhysicsAsset*& InOutPhysicsAsset)
{
	const FString PopupId = "Physics Asset Build Options##PhysicsAssetBuildOptions_" + std::to_string(GetOwnerInstanceId());

	if (bOpenPhysicsAssetBuildOptions)
	{
		ImGui::OpenPopup(PopupId.c_str());
		bOpenPhysicsAssetBuildOptions = false;
	}

	if (!ImGui::BeginPopupModal(PopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::PushID(PopupId.c_str());

	ImGui::TextUnformatted("Physics Asset Build Options");
	ImGui::Separator();

	ImGui::Checkbox("Use Dominant Bone Weight", &PendingPhysicsAssetBuildOptions.bUseDominantBoneWeight);
	ImGui::Checkbox("Auto Orient To Bone", &PendingPhysicsAssetBuildOptions.bAutoOrientToBone);
	ImGui::Checkbox("Walk Past Small Bones", &PendingPhysicsAssetBuildOptions.bWalkPastSmall);
	ImGui::Checkbox("Create Body for All Bones", &PendingPhysicsAssetBuildOptions.bBodyForAll);

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

	if (ImGui::BeginCombo("Primitive Type", GetGeomTypeLabel(PendingPhysicsAssetBuildOptions.GeomType)))
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

	ImGui::Spacing();
	ImGui::DragFloat("Min Bone Size", &PendingPhysicsAssetBuildOptions.MinBoneSize, 0.25f, 0.0f, 1000.0f, "%.2f");
	ImGui::DragFloat("Min Weld Size", &PendingPhysicsAssetBuildOptions.MinWeldSize, 0.0001f, 0.0f, 1000.0f, "%.4f");
	ImGui::DragFloat("Fit Padding", &PendingPhysicsAssetBuildOptions.FitPadding, 0.001f, 1.0f, 2.0f, "%.3f");
	ImGui::DragFloat("Min Primitive Size", &PendingPhysicsAssetBuildOptions.MinPrimitiveSize, 0.01f, 0.01f, 1000.0f, "%.2f");

	ImGui::Separator();

	if (!SkeletalMesh)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Generate"))
	{
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
			ImGui::CloseCurrentPopup();
		}
	}

	if (!SkeletalMesh)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		PendingPhysicsAssetBuildOptions = FPhysicsAssetBuildOptions {};
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::PopID();
	ImGui::EndPopup();
}

void FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBodyList(USkeletalMesh* SkeletalMesh, UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		ImGui::TextDisabled("No physics asset.");
		return;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (SelectedPhysicsBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		SelectedPhysicsBodyIndex = -1;
		SelectedPhysicsConstraintIndex = -1;
	}
	if (SelectedPhysicsConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintInitDescs().size()))
	{
		SelectedPhysicsConstraintIndex = -1;
	}
	SyncDebugComponent(PhysicsAsset);

	if (ImGui::Selectable("Physics Asset##PhysicsAssetRoot", SelectedPhysicsBodyIndex < 0 && SelectedPhysicsConstraintIndex < 0))
	{
		SelectedPhysicsBodyIndex = -1;
		SelectedPhysicsConstraintIndex = -1;
		SyncDebugComponent(PhysicsAsset);
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
	}

	if (ImGui::BeginPopupContextItem())
	{
		TArray<UBodySetup*>& MutableBodies = PhysicsAsset->GetBodySetupsMutable();
		UBodySetup* MutableBody = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(MutableBodies.size()))
			? MutableBodies[BodyIndex]
			: nullptr;
		if (RenderAddPhysicsBodyShapeMenu(MutableBody))
		{
			SelectedPhysicsBodyIndex = BodyIndex;
			SelectedPhysicsConstraintIndex = -1;
			SavePhysicsAssetChange("PhysicsAsset body shape add warning");
			MarkDirty();
			SyncDebugComponent(PhysicsAsset);
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

void FMeshEditorPhysicsAssetTab::RenderPhysicsAssetBodyDetails(UPhysicsAsset* PhysicsAsset)
{
	if (RenderConstraintInitDescDetails(
		PhysicsAsset,
		SelectedPhysicsConstraintIndex,
		GetViewportClient().GetPhysicsAssetDebugComponent()))
	{
		SavePhysicsAssetChange("PhysicsAsset constraint edit warning");
		MarkDirty();
		return;
	}

	if (SelectedPhysicsConstraintIndex < 0)
	{
		const bool bChanged = SelectedPhysicsBodyIndex >= 0
			? RenderBodySetupDetails(PhysicsAsset, SelectedPhysicsBodyIndex)
			: RenderPhysicsAssetDetails(PhysicsAsset);
		if (bChanged)
		{
			SavePhysicsAssetChange("PhysicsAsset body edit warning");
			MarkDirty();
			SyncDebugComponent(PhysicsAsset);
			if (UPhysicsAssetDebugComponent* DebugComponent = GetViewportClient().GetPhysicsAssetDebugComponent())
			{
				DebugComponent->MarkPhysicsAssetDebugDirty();
			}
		}
	}
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
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetShapeEdited()
{
	SavePhysicsAssetChange("PhysicsAsset shape edit warning");
	MarkDirty();
}

void FMeshEditorPhysicsAssetTab::OnPhysicsAssetConstraintEdited()
{
	SavePhysicsAssetChange("PhysicsAsset constraint gizmo warning");
	MarkDirty();
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
