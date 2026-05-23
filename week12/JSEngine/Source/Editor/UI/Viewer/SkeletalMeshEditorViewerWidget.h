#pragma once

#include "Editor/UI/Viewer/EditorViewerWidget.h"
#include "Asset/SkeletalMeshTypes.h"

class USkeletalMeshComponent;

class FSkeletalMeshEditorViewerWidget : public FEditorViewerWidget
{
public:
	~FSkeletalMeshEditorViewerWidget() override = default;

	void RequestSaveMesh() override;
	bool CanSaveMesh() const override;
	bool IsMeshDirty() const override;

protected:
	void RenderContent(float DeltaTime) override;

private:
	// Core Data & Rendering
	FSkeletalMesh* ResolveCurrentMeshData() const;
	
	void RenderBoneDetails(USkeletalMeshComponent* SkelComp);
	void RenderSkeletonLeftPanel(USkeletalMeshComponent* SkelMeshComp, FSkeletalMesh* MeshData);
	void RenderBoneRightPanel(USkeletalMeshComponent* SkelMeshComp);

	void DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children);
	void DrawSocketNode(int32 SocketIdx);
	void DrawSocketInspector();
	void DrawPreviewPickerModal();
	void DrawRenameModal();

	// Bone Tree Management
	void RebuildBoneTreeCaches(const FSkeletalMesh* MeshData);
	void QueueBoneSubtreeOpenState(int32 BoneIdx, bool bOpen);
	void ApplyPendingBoneTreeOpenState(const FSkeletalMesh* MeshData);
	void SetBoneSubtreeOpenState(int32 BoneIdx, const TArray<TArray<int32>>& InChildren, bool bOpen);

	// Socket Management
	void RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData);
	void AddSocketOnBone(int32 BoneIdx);
	void DeleteSocket(int32 SocketIdx);
	FString GenerateUniqueSocketName(const char* Base = "Socket") const;
	bool IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const;
	bool HasPreview(const FName& SocketName) const;

	// State & Saving
	void TriggerSaveMesh();
	uint64 ComputeEditableMeshSignature(const FSkeletalMesh* MeshData) const;
	void ResetMeshDirtyBaseline();
	bool HasMeshAssetEdits() const;

private:
	// Cached Data
	FSkeletalMesh* CachedMesh = nullptr;
	USkeletalMeshComponent* CachedSkComp = nullptr;

	TArray<TArray<int32>> Children;
	TArray<TArray<int32>> BoneToSocketIndices;

	// State Variables
	uint64 CleanMeshEditSignature = 0;

	int32 PendingPreviewPickerSocketIdx = -1;
	int32 RenameSocketIdx = -1;
	int32 PendingBoneTreeOpenStateRoot = -1;

	char RenameBuffer[256] = {};

	bool bPendingBoneTreeOpenStateValue = false;
	bool bMeshDirty = false;
	bool bHasCleanMeshEditSignature = false;
};
