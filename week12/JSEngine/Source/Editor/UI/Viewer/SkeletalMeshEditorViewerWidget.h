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
	void RebuildBoneTreeCaches(const FSkeletalMesh* MeshData);
	void RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData);
	void QueueBoneSubtreeOpenState(int32 BoneIdx, bool bOpen);
	void ApplyPendingBoneTreeOpenState(const FSkeletalMesh* MeshData);
	void SetBoneSubtreeOpenState(int32 BoneIdx, const TArray<TArray<int32>>& InChildren, bool bOpen);
	void AddSocketOnBone(int32 BoneIdx);
	void DeleteSocket(int32 SocketIdx);
	bool HasPreview(const FName& SocketName) const;
	FString GenerateUniqueSocketName(const char* Base = "Socket") const;
	void DrawPreviewPickerModal();
	void DrawSocketInspector();
	void TriggerSaveMesh();
	void DrawRenameModal();
	bool IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const;
	void RenderBoneDetails(USkeletalMeshComponent* SkelComp);
	void RenderSkeletonLeftPanel(USkeletalMeshComponent* SkelMeshComp, FSkeletalMesh* MeshData);
	void RenderBoneRightPanel(USkeletalMeshComponent* SkelMeshComp);
	void DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children);
	void DrawSocketNode(int32 SocketIdx);
	FSkeletalMesh* ResolveCurrentMeshData() const;
	uint64 ComputeEditableMeshSignature(const FSkeletalMesh* MeshData) const;
	void ResetMeshDirtyBaseline();
	bool HasMeshAssetEdits() const;

	TArray<TArray<int32>> Children;
	TArray<TArray<int32>> BoneToSocketIndices;
	FSkeletalMesh* CachedMesh = nullptr;
	USkeletalMeshComponent* CachedSkComp = nullptr;

	int32 PendingPreviewPickerSocketIdx = -1;
	int32 RenameSocketIdx = -1;
	int32 PendingBoneTreeOpenStateRoot = -1;
	bool bPendingBoneTreeOpenStateValue = false;
	char RenameBuffer[256] = {};
	bool bMeshDirty = false;
	uint64 CleanMeshEditSignature = 0;
	bool bHasCleanMeshEditSignature = false;
};
