#include "SkeletalMesh.h"

#include "Core/Logging/Log.h"

DEFINE_CLASS(USkeletalMesh, UObject)

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

const TArray<FMatrix>& USkeletalMesh::GetInverseBindPoses() const
{
    static const TArray<FMatrix> Empty = {};
    return MeshData ? MeshData->InverseBindPoseMatrices : Empty;
}

const TArray<FMatrix>& USkeletalMesh::GetReferenceLocalPose() const
{
    static const TArray<FMatrix> Empty = {};
    return MeshData ? MeshData->ReferenceLocalPose : Empty;
}

const TArray<FMatrix>& USkeletalMesh::GetReferenceGlobalPose() const
{
    static const TArray<FMatrix> Empty = {};
    return MeshData ? MeshData->ReferenceGlobalPose : Empty;
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