#pragma once
#include "Object/Object.h"
#include "Skeleton.h"
#include "SkeletalMeshTypes.h"

#include "SkeletalMesh.generated.h"

UCLASS()
class USkeletalMesh : public UObject
{
    GENERATED_BODY_USkeletalMesh()
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh() = default;
    ~USkeletalMesh() override;

    void SetMeshData(FSkeletalMesh* InMeshData);

    FSkeletalMesh* GetMeshData();
    const FSkeletalMesh* GetMeshData() const;

    const FString& GetAssetPathFileName() const;

    const FSkeletalMeshRenderData* GetResourceForRendering() const;
    const FSkeletalMeshLODRenderData* GetLODRenderData(int32 LODIndex = 0) const;

    const TArray<FSkeletalMeshVertex>& GetVertices(int32 LODIndex = 0) const;
    const TArray<uint32>& GetIndices(int32 LODIndex = 0) const;

    const TArray<FBoneInfo>& GetBones() const;

	const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;

    const FMatrix& GetLocalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetGlobalBindTransform(int32 BoneIndex) const;
    const FMatrix& GetInverseBindPose(int32 BoneIndex) const;

    const TArray<FSkeletalMeshRenderSection>& GetRenderSections(int32 LODIndex = 0) const;
    const TArray<FSkeletalMeshRenderSection>& GetSections(int32 LODIndex = 0) const;
    const TArray<FStaticMeshMaterialSlot>& GetMaterialSlots() const;

    // Sockets
    const TArray<FSkeletalMeshSocket>& GetSockets() const;
    const FSkeletalMeshSocket*         FindSocket(const FName& Name) const;
    bool                               HasSocket(const FName& Name) const;

    const FAABB& GetLocalBounds() const;

    bool HasValidMeshData() const;
    void SetSkeleton(USkeleton* InSkeleton, bool bTakeOwnership = false);
    USkeleton* GetSkeleton() const;
    const FReferenceSkeleton* GetReferenceSkeleton() const;

private:
    void RebuildLocalBoundsFromMeshData();
    void ReleaseOwnedSkeleton();

private:
    USkeleton* Skeleton = nullptr;
    bool bOwnsSkeleton = false;
    FSkeletalMesh* MeshData = nullptr;
};
