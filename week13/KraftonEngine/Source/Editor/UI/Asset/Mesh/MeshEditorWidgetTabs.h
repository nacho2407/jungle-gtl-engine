#pragma once

#include "MeshEditorWidgetTab.h"
#include "Asset/AssetRegistry.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"
#include "Math/Vector.h"
#include "PhysicsEngine/PhysicsAssetBuilder.h"

class UAnimMontage;
class UAnimSequence;
class UPhysicsAsset;
struct FSkeletalMesh;

struct FAnimationTabState
{
	UAnimSequence* CurrentSequence = nullptr;
	UAnimMontage* CurrentMontage = nullptr;
	int32 SelectedAnimIndex = -1;
	int32 SelectedMontageIndex = -1;
	bool bMontageSelected = false;

	int32 SelectedNotifyIndex = -1;
	int32 SelectedMorphCurveIndex = -1;
	int32 SelectedMorphKeyIndex = -1;
	TArray<float> MorphPreviewWeights;
	TArray<uint8> MorphPreviewOverrideMask;
	bool bMorphPreviewOverrideEnabled = false;

	TArray<FAssetListItem> CachedAnimationFiles;
	TArray<FAssetListItem> CachedMontageFiles;
	FSkeletonBinding CachedAnimationListBinding;
	bool bAnimationListDirty = true;
	bool bMontagesScanned = false;

	float AnimListWidth = 200.0f;
	float AnimDetailsWidth = 280.0f;

	FFbxAnimationImportDialogState AnimationImportDialog;
};

class FMeshEditorSkeletonTab : public FMeshEditorWidgetTab
{
public:
	explicit FMeshEditorSkeletonTab(FMeshEditorWidget& InOwner);

	EMeshEditorTab GetType() const override { return EMeshEditorTab::Skeleton; }
	const char* GetLabel() const override { return "Skeleton"; }
	const wchar_t* GetIconFileName() const override { return L"Skeleton.png"; }

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;
	bool ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const override;

	void Render(float AvailableHeight) override;
	void Reset() override;
	void OnPreviewActorCreated(AActor* Actor) override;
	void OnEditorOpened() override;
	void OnActivated(EMeshEditorTab PreviousTab) override;

private:
	void RenderBoneTree(const FSkeletalMesh* Asset, int32 Index);
	int32 GetSelectedBoneIndexForViewport() const override { return SelectedBoneIndex; }

private:
	int32 SelectedBoneIndex = -1;
	float HierarchyWidth = 250.0f;
	float DetailsWidth = 300.0f;
};

class FMeshEditorMeshTab : public FMeshEditorWidgetTab
{
public:
	explicit FMeshEditorMeshTab(FMeshEditorWidget& InOwner);

	EMeshEditorTab GetType() const override { return EMeshEditorTab::Mesh; }
	const char* GetLabel() const override { return "Mesh"; }
	const wchar_t* GetIconFileName() const override { return L"SkeletalMesh.png"; }

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Render(float AvailableHeight) override;
};

class FMeshEditorAnimationTab : public FMeshEditorWidgetTab
{
public:
	explicit FMeshEditorAnimationTab(FMeshEditorWidget& InOwner);

	EMeshEditorTab GetType() const override { return EMeshEditorTab::Animation; }
	const char* GetLabel() const override { return "Animation"; }
	const wchar_t* GetIconFileName() const override { return L"Animation.png"; }

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Render(float AvailableHeight) override;
	void Tick(float DeltaTime) override;
	void Reset() override;
	void OnEditorOpened() override;

private:
	void ApplyAnimationToComponent();
	void ResetMorphPreviewOverrides();
	void EnsureMorphPreviewOverrideSize();
	void ApplyMorphPreviewOverrides(TArray<float>& InOutMorphWeights) const;
	void RefreshAnimationPreviewPose();
	void MarkAnimationListDirty();
	const TArray<FAssetListItem>& GetCachedAnimationFilesForCurrentSkeleton();
	const TArray<FAssetListItem>& GetCachedMontageFilesForCurrentSkeleton();

private:
	FAnimationTabState AnimTabState;
};

class FMeshEditorPhysicsAssetTab : public FMeshEditorWidgetTab
{
public:
	explicit FMeshEditorPhysicsAssetTab(FMeshEditorWidget& InOwner);

	EMeshEditorTab GetType() const override { return EMeshEditorTab::PhysicsAsset; }
	const char* GetLabel() const override { return "Physics Asset"; }
	const wchar_t* GetIconFileName() const override { return L"Show_Flag.png"; }

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;
	bool ShouldActivateOnReuse(UObject* Object) const override;
	bool ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const override;

	void Render(float AvailableHeight) override;
	void Tick(float DeltaTime) override;
	void Reset() override;
	void OnEditorOpened() override;
	void OnEditorClosing() override;
	void OnActivated(EMeshEditorTab PreviousTab) override;
	void OnDeactivated(EMeshEditorTab NextTab) override;

	void OnPhysicsAssetBodyPicked(int32 BodyIndex) override;
	void OnPhysicsAssetConstraintPicked(int32 ConstraintIndex) override;
	void OnPhysicsAssetShapeEdited() override;
	void OnPhysicsAssetConstraintEdited() override;

private:
	enum class EPhysicsAssetDetailTargetType : uint8
	{
		None,
		PhysicsAsset,
		Body,
		Constraint
	};

	void RenderPhysicsAssetBuildOptionsPopup(USkeletalMesh* SkeletalMesh, UPhysicsAsset*& InOutPhysicsAsset);
	void RenderPhysicsAssetBodyList(USkeletalMesh* SkeletalMesh, UPhysicsAsset* PhysicsAsset);
	bool RenderPhysicsAssetBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex);
	void SyncReflectionDetailTarget(UPhysicsAsset* PhysicsAsset);
	void ClearReflectionDetailTarget();
	void RefreshReflectionDetailSnapshot(UPhysicsAsset* PhysicsAsset);
	void DetectReflectionDetailChanges(UPhysicsAsset* PhysicsAsset);
	void SavePhysicsAssetChange(const char* LogPrefix);
	void SyncDebugComponent(UPhysicsAsset* PhysicsAsset);

private:
	int32 SelectedPhysicsBodyIndex = -1;
	int32 SelectedPhysicsConstraintIndex = -1;
	EPhysicsAssetDetailTargetType ReflectionDetailTargetType = EPhysicsAssetDetailTargetType::None;
	FSelectionDetailTarget ReflectionDetailTarget;
	TArray<uint8> ReflectionDetailSnapshot;
	bool bHasReflectionDetailSnapshot = false;
	FVector SnapshotConstraintParentLocation = FVector::ZeroVector;
	FVector SnapshotConstraintChildLocation = FVector::ZeroVector;
	bool bOpenPhysicsAssetBuildOptions = false;
	FPhysicsAssetBuildOptions PendingPhysicsAssetBuildOptions;
};
