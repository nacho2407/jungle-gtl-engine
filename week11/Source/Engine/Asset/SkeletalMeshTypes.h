#pragma once
#include "Core/CoreMinimal.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "StaticMeshTypes.h"

static constexpr int32 MAX_GPUSKIN_BONES_PER_SECTION = 256;
static constexpr int32 MAX_BONE_INFLUENCES = 4;

using FBoneIndexType = int32;
using FSectionBoneIndexType = uint8;

struct FBoneInfo
{
    FName Name;

    int32 ParentIndex = -1;

    FMatrix LocalBindTransform;
    FMatrix GlobalBindTransform;

    FMatrix InverseBindPose;
};

// 본에 묶인 명명된 attach point. T/R/S는 본 local 기준.
struct FSkeletalMeshSocket
{
    FName    Name;
    int32    BoneIndex      = -1;
    FVector  RelativeLocation = FVector::ZeroVector;
    FRotator RelativeRotation;                              // 기본값 = (0,0,0)
    FVector  RelativeScale  = FVector(1.0f, 1.0f, 1.0f);

    // T·R·S → 4x4 (row-vector 규약: v · S · R · T)
    FMatrix GetRelativeTransform() const;
};
struct FReferenceSkeleton
{
    TArray<FBoneInfo> RefBones;
    TMap<FName, int32> BoneNameToIndex;
    void RebuildNameToIndex();
    int32 FindBoneIndex(const FName& BoneName) const;
};

struct FSkeletalMeshRenderSection
{
    uint32 BaseIndex = 0;
    uint32 IndexCount = 0;
    uint32 BaseVertexIndex = 0;
    uint32 NumVertices = 0;
    uint32 NumTriangles = 0;

    int32 MaterialIndex = -1;
    int32 MaxBoneInfluences = MAX_BONE_INFLUENCES;

    TArray<FBoneIndexType> BoneMap;
};

struct FSkeletalMeshLODRenderData
{
    TArray<FSkeletalMeshVertex> StaticVertices;
    TArray<uint32> Indices;
    TArray<FSkeletalMeshRenderSection> RenderSections;

    TArray<FBoneIndexType> ActiveBoneIndices;
    TArray<FBoneIndexType> RequiredBones;
};

struct FSkeletalMeshRenderData
{
    TArray<FSkeletalMeshLODRenderData> LODRenderData;
};

struct FSkeletalMesh
{
    FString PathFileName;

    FSkeletalMeshRenderData RenderData;

    // 본에 연결되는 명명된 attach point들. asset 영속 데이터.
    TArray<FSkeletalMeshSocket> Sockets;

    // Material
    TArray<FStaticMeshMaterialSlot> MaterialSlots;

    // Bounds
    FAABB LocalBounds;
};

struct FSkeletalMeshImportData
{
    FSkeletalMesh* MeshData = nullptr;
    FReferenceSkeleton ReferenceSkeleton;
};
