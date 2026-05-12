#pragma once
#include "Core/CoreMinimal.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "StaticMeshTypes.h"

struct FBoneInfo
{
    FString Name;

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

struct FSkeletalMesh
{
    FString PathFileName;
    TArray<FSkeletalMeshVertex> Vertices;
    TArray<uint32> Indices;

    TArray<FBoneInfo> Bones;

    // 변환 행렬의 경우 FBoneInfo에만 두도록 처리

    // 본에 연결되는 명명된 attach point들. asset 영속 데이터.
    TArray<FSkeletalMeshSocket> Sockets;

    // Material
	// StaticMeshSection 이긴 하나, StaticMesh 에 종속된 개념이 아니라 이름은 나중에 바꿔야할것으로 보임
    TArray<FStaticMeshSection> Sections;
    TArray<FStaticMeshMaterialSlot> MaterialSlots;

    // Bounds
    FAABB LocalBounds;
};