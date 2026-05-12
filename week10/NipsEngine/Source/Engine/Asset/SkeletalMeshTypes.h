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

    // 변환 행렬의 경우 FBoneInfo에만 두도록 처리

    // Material
	// StaticMeshSection 이긴 하나, StaticMesh 에 종속된 개념이 아니라 이름은 나중에 바꿔야할것으로 보임
    TArray<FStaticMeshSection> Sections;
    TArray<FStaticMeshMaterialSlot> MaterialSlots;

    // Bounds
    FAABB LocalBounds;
};