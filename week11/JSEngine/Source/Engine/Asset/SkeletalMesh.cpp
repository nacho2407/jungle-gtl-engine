#include "SkeletalMesh.h"

#include "Core/Logging/Log.h"
#include "Engine/Geometry/Transform.h"

DEFINE_CLASS(USkeletalMesh, UObject)

FMatrix FSkeletalMeshSocket::GetRelativeTransform() const
{
    // row-vector 규약: v · S · R · T  (FTransform::ToMatrixWithScale가 동일 합성)
    return FTransform(RelativeRotation, RelativeLocation, RelativeScale).ToMatrixWithScale();
}

USkeletalMesh::~USkeletalMesh()
{
    if (MeshData != nullptr)
    {
        delete MeshData;
        MeshData = nullptr;
    }

    ReleaseOwnedSkeleton();
}

void USkeletalMesh::SetMeshData(FSkeletalMesh* InMeshData)
{
    if (MeshData == InMeshData)
    {
        return;
    }

    delete MeshData;
    MeshData = InMeshData;

    RebuildLocalBoundsFromMeshData();
}

FSkeletalMesh* USkeletalMesh::GetMeshData()
{
    return MeshData;
}

const FSkeletalMesh* USkeletalMesh::GetMeshData() const
{
    return MeshData;
}

const FString& USkeletalMesh::GetAssetPathFileName() const
{
    static FString Empty = {};
    return MeshData ? MeshData->PathFileName : Empty;
}

const FSkeletalMeshRenderData* USkeletalMesh::GetResourceForRendering() const
{
    return MeshData ? &MeshData->RenderData : nullptr;
}

const FSkeletalMeshLODRenderData* USkeletalMesh::GetLODRenderData(int32 LODIndex) const
{
    if (!MeshData || LODIndex < 0 || LODIndex >= static_cast<int32>(MeshData->RenderData.LODRenderData.size()))
    {
        return nullptr;
    }

    return &MeshData->RenderData.LODRenderData[LODIndex];
}

const TArray<FSkeletalMeshVertex>& USkeletalMesh::GetVertices(int32 LODIndex) const
{
    static const TArray<FSkeletalMeshVertex> Empty = {};
    const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(LODIndex);
    return LODData ? LODData->StaticVertices : Empty;
}

const TArray<uint32>& USkeletalMesh::GetIndices(int32 LODIndex) const
{
    static const TArray<uint32> Empty = {};
    const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(LODIndex);
    return LODData ? LODData->Indices : Empty;
}

const TArray<FBoneInfo>& USkeletalMesh::GetBones() const
{
    static const TArray<FBoneInfo> Empty = {};
    const FReferenceSkeleton* RefSkeleton = GetReferenceSkeleton();

	if (RefSkeleton)
        return RefSkeleton->RefBones;
    return Empty;
}

const FBoneInfo* USkeletalMesh::GetBoneInfo(int32 BoneIndex) const
{
    const TArray<FBoneInfo>& Bones = GetBones();

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return nullptr;
    }

    return &Bones[BoneIndex];
}

const FMatrix& USkeletalMesh::GetLocalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->LocalBindTransform : Identity;
}

const FMatrix& USkeletalMesh::GetGlobalBindTransform(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->GlobalBindTransform : Identity;
}

const FMatrix& USkeletalMesh::GetInverseBindPose(int32 BoneIndex) const
{
    static const FMatrix Identity = FMatrix::Identity;

    const FBoneInfo* Bone = GetBoneInfo(BoneIndex);
    return Bone ? Bone->InverseBindPose : Identity;
}

const TArray<FSkeletalMeshRenderSection>& USkeletalMesh::GetRenderSections(int32 LODIndex) const
{
    static const TArray<FSkeletalMeshRenderSection> Empty = {};
    const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(LODIndex);
    return LODData ? LODData->RenderSections : Empty;
}

const TArray<FSkeletalMeshRenderSection>& USkeletalMesh::GetSections(int32 LODIndex) const
{
    return GetRenderSections(LODIndex);
}

const TArray<FStaticMeshMaterialSlot>& USkeletalMesh::GetMaterialSlots() const
{
    static const TArray<FStaticMeshMaterialSlot> Empty = {};
    return MeshData ? MeshData->MaterialSlots : Empty;
}

const TArray<FSkeletalMeshSocket>& USkeletalMesh::GetSockets() const
{
    static const TArray<FSkeletalMeshSocket> Empty = {};
    return MeshData ? MeshData->Sockets : Empty;
}

const FSkeletalMeshSocket* USkeletalMesh::FindSocket(const FName& Name) const
{
    if (!MeshData || !Name.IsValid())
    {
        return nullptr;
    }

    for (const FSkeletalMeshSocket& Socket : MeshData->Sockets)
    {
        if (Socket.Name == Name)
        {
            return &Socket;
        }
    }

    return nullptr;
}

bool USkeletalMesh::HasSocket(const FName& Name) const
{
    return FindSocket(Name) != nullptr;
}

const FAABB& USkeletalMesh::GetLocalBounds() const
{
    static const FAABB Empty = {};
    return MeshData ? MeshData->LocalBounds : Empty;
}

bool USkeletalMesh::HasValidMeshData() const
{
    const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(0);
	return MeshData != nullptr &&
        LODData != nullptr &&
        !LODData->StaticVertices.empty() &&
        !LODData->Indices.empty() &&
        !LODData->RenderSections.empty() &&
        !GetBones().empty();
}

void USkeletalMesh::SetSkeleton(USkeleton* InSkeleton, bool bTakeOwnership)
{
    if (Skeleton == InSkeleton)
    {
        bOwnsSkeleton = bTakeOwnership && InSkeleton != nullptr;
        return;
    }

    ReleaseOwnedSkeleton();
    Skeleton = InSkeleton;
    bOwnsSkeleton = bTakeOwnership && InSkeleton != nullptr;
}

USkeleton* USkeletalMesh::GetSkeleton() const
{
    return Skeleton;
}

const FReferenceSkeleton* USkeletalMesh::GetReferenceSkeleton() const
{
    if (Skeleton)
        return &Skeleton->GetReferenceSkeleton();
    return nullptr;
}

void USkeletalMesh::RebuildLocalBoundsFromMeshData()
{
    if (!MeshData)
    {
        return;
    }

    MeshData->LocalBounds.Reset();

    const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(0);
    if (!LODData)
    {
        return;
    }

    for (const FSkeletalMeshVertex& Vertex : LODData->StaticVertices)
    {
        MeshData->LocalBounds.Expand(Vertex.Position);
    }
}

void USkeletalMesh::ReleaseOwnedSkeleton()
{
    if (Skeleton && bOwnsSkeleton)
    {
        UObjectManager::Get().DestroyObject(Skeleton);
    }

    Skeleton = nullptr;
    bOwnsSkeleton = false;
}
