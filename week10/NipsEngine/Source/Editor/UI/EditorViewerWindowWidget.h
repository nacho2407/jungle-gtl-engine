#pragma once
#include "Editor/UI/EditorWidget.h"
#include "Asset/SkeletalMeshTypes.h"

class USkeletalMeshComponent;

class FEditorViewerWindowWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;
    void DrawBoneNode(
        class FEditorViewer& Viewer,
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const TArray<TArray<int32>>& Children);

    // Bone tree에 socket을 leaf 노드로 표시. SocketIdx는 CachedMesh->Sockets 배열 인덱스.
    void DrawSocketNode(FEditorViewer& Viewer, int32 SocketIdx);

private:
    // bone tree 캐시들. CachedMesh가 바뀌면 둘 다 재빌드.
    // socket을 add/delete할 때도 BoneToSocketIndices만 다시 빌드해야 한다.
    void RebuildBoneTreeCaches(const FSkeletalMesh* MeshData);
    void RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData);

    // Socket 편집 액션들. Render()안에서만 호출 — CachedMesh/CachedSkComp가 유효한 시점.
    void    AddSocketOnBone(int32 BoneIdx);
    void DeleteSocket(FEditorViewer& Viewer, int32 SocketIdx);
    bool HasPreview(FEditorViewer& Viewer, const FName& SocketName) const;
    FString GenerateUniqueSocketName(const char* Base = "Socket") const;

    // 모달 picker — Render() 끝에서 호출하여 그림.
    void DrawPreviewPickerModal(FEditorViewer& Viewer);

    // 좌측 패널 하단의 선택된 socket properties 편집기 + Save Mesh 버튼.
    void    DrawSocketInspector();
    void    TriggerSaveMesh();

    // Rename 다이얼로그
    void DrawRenameModal(FEditorViewer& Viewer);
    bool    IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const;

	void RenderBoneDetails(FEditorViewer& Viewer, USkeletalMeshComponent* SkelComp);

    TArray<TArray<int32>> Children;             // bone idx → child bone indices
    TArray<TArray<int32>> BoneToSocketIndices;  // bone idx → socket array indices
    FSkeletalMesh* CachedMesh = nullptr;
    USkeletalMeshComponent* CachedSkComp = nullptr;   // Render() 내내만 유효한 transient cache

    int32 SelectedBoneIndex = -1;
    int32 SelectedSocketIndex = -1;   // -1이면 미선택. SelectedBoneIndex와 상호 배타.
    int32 PendingPreviewPickerSocketIdx = -1;  // picker modal 트리거; -1이면 닫힌 상태
    int32 RenameSocketIdx = -1;                // rename modal 트리거; -1이면 닫힌 상태
    char  RenameBuffer[256] = {};
    bool  bMeshDirty = false;         // socket 데이터 변경 후 Save 트리거용
};
