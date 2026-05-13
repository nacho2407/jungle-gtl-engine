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

const TArray<FSkeletalMeshVertex>& USkeletalMesh::GetVertices() const
{
    static const TArray<FSkeletalMeshVertex> Empty = {};
    return MeshData ? MeshData->Vertices : Empty;
}

const TArray<uint32>& USkeletalMesh::GetIndices() const
{
    static const TArray<uint32> Empty = {};
    return MeshData ? MeshData->Indices : Empty;
}

const TArray<FBoneInfo>& USkeletalMesh::GetBones() const
{
    static const TArray<FBoneInfo> Empty = {};
    return MeshData ? MeshData->Bones : Empty;
}

const FBoneInfo* USkeletalMesh::GetBoneInfo(int32 BoneIndex) const
{
    if (!MeshData)
    {
        return nullptr;
    }

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshData->Bones.size()))
    {
        return nullptr;
    }

    return &MeshData->Bones[BoneIndex];
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

const TArray<FStaticMeshSection>& USkeletalMesh::GetSections() const
{
    static const TArray<FStaticMeshSection> Empty = {};
    return MeshData ? MeshData->Sections : Empty;
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
    return MeshData != nullptr && !MeshData->Vertices.empty() && !MeshData->Indices.empty() && !MeshData->Bones.empty();
}

void USkeletalMesh::RebuildLocalBoundsFromMeshData()
{
    if (!MeshData)
    {
        return;
    }

    MeshData->LocalBounds.Reset();

    for (const FSkeletalMeshVertex& Vertex : MeshData->Vertices)
    {
        MeshData->LocalBounds.Expand(Vertex.Position);
    }
}