#pragma once
#include "Object/Object.h"
#include "SkeletalMeshTypes.h"

class USkeletalMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh() = default;
    ~USkeletalMesh() override;

    void SetMeshData(FSkeletalMesh* InMeshData);

    FSkeletalMesh* GetMeshData();
    const FSkeletalMesh* GetMeshData() const;

    const FString& GetAssetPathFileName() const;

    const TArray<FSkeletalMeshVertex>& GetVertices() const;
    const TArray<uint32>& GetIndices() const;

    const TArray<FBoneInfo>& GetBones() const;

	const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;

    const FMatrix& GetLocalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetGlobalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetInverseBindPose(int32 BoneIndex) const;

    const TArray<FStaticMeshSection>& GetSections() const;
    const TArray<FStaticMeshMaterialSlot>& GetMaterialSlots() const;

    const FAABB& GetLocalBounds() const;

    bool HasValidMeshData() const;

private:
    void RebuildLocalBoundsFromMeshData();

private:
    FSkeletalMesh* MeshData = nullptr;
};