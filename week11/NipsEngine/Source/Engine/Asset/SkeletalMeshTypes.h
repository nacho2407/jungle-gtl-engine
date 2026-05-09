#pragma once
#include "Core/CoreMinimal.h"
#include "StaticMeshTypes.h"

struct FBoneInfo
{
    FString Name;

    int32 ParentIndex = -1;

    FMatrix LocalBindTransform;
    FMatrix GlobalBindTransform;

    FMatrix InverseBindPose;
};

struct FSkeletalMesh
{
    FString PathFileName;
    TArray<FSkeletalMeshVertex> Vertices;
    TArray<uint32> Indices;

    TArray<FBoneInfo> Bones;

    TArray<FMatrix> InverseBindPoseMatrices;

    // Reference Pose
    TArray<FMatrix> ReferenceLocalPose;
    TArray<FMatrix> ReferenceGlobalPose;

    // Material
	// StaticMeshSection 이긴 하나, StaticMesh 에 종속된 개념이 아니라 이름은 나중에 바꿔야할것으로 보임
    TArray<FStaticMeshSection> Sections;
    TArray<FStaticMeshMaterialSlot> MaterialSlots;

    // Bounds
    FAABB LocalBounds;
};