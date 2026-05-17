#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"

inline void SerializeReferenceSkeletonMatrix(FArchive& Ar, FMatrix& Matrix)
{
    Ar.Serialize(Matrix.Data, sizeof(float) * 16);
}

struct FReferenceBone
{
    FString Name;
    int32   ParentIndex = -1;

    FMatrix LocalBindPose   = FMatrix::Identity;
    FMatrix GlobalBindPose  = FMatrix::Identity;
    FMatrix InverseBindPose = FMatrix::Identity;

    friend FArchive& operator<<(FArchive& Ar, FReferenceBone& Bone)
    {
        Ar << Bone.Name;
        Ar << Bone.ParentIndex;
        SerializeReferenceSkeletonMatrix(Ar, Bone.LocalBindPose);
        SerializeReferenceSkeletonMatrix(Ar, Bone.GlobalBindPose);
        SerializeReferenceSkeletonMatrix(Ar, Bone.InverseBindPose);
        return Ar;
    }
};

struct FReferenceSkeleton
{
    TArray<FReferenceBone> Bones;

    int32 GetNumBones() const
    {
        return static_cast<int32>(Bones.size());
    }

    int32 FindBoneIndex(const FString& BoneName) const
    {
        for (int32 BoneIndex = 0; BoneIndex < GetNumBones(); ++BoneIndex)
        {
            if (Bones[BoneIndex].Name == BoneName)
            {
                return BoneIndex;
            }
        }

        return -1;
    }

    friend FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& Skeleton)
    {
        Ar << Skeleton.Bones;
        return Ar;
    }
};

// Skeleton을 참조하는 모든 에셋이 공통으로 저장하는 연결 정보.
// Path는 로딩용, AssetGuid는 같은 Skeleton 에셋 판별용, CompatibilitySignature는 구조 호환성 판별용이다.
struct FSkeletonBinding
{
    FString SkeletonPath = "None";
    FString SkeletonAssetGuid;
    FString CompatibilitySignature;

    bool HasSkeletonPath() const
    {
        return !SkeletonPath.empty() && SkeletonPath != "None";
    }

    bool HasAssetGuid() const
    {
        return !SkeletonAssetGuid.empty();
    }

    bool HasCompatibilitySignature() const
    {
        return !CompatibilitySignature.empty();
    }

    void Reset()
    {
        SkeletonPath = "None";
        SkeletonAssetGuid.clear();
        CompatibilitySignature.clear();
    }

    friend FArchive& operator<<(FArchive& Ar, FSkeletonBinding& Binding)
    {
        Ar << Binding.SkeletonPath;
        Ar << Binding.SkeletonAssetGuid;
        Ar << Binding.CompatibilitySignature;
        return Ar;
    }
};

enum class ESkeletonCompatibilityResult : uint8
{
    Incompatible = 0,
    ExactSkeleton,
    SameStructure,
    Retargetable
};

struct FSkeletonCompatibilityReport
{
    ESkeletonCompatibilityResult Result = ESkeletonCompatibilityResult::Incompatible;
    FString Reason;
    TArray<FString> MissingBones;
    TArray<FString> ExtraBones;
    TArray<FString> ParentMismatchBones;

    bool IsCompatible() const
    {
        return Result == ESkeletonCompatibilityResult::ExactSkeleton
            || Result == ESkeletonCompatibilityResult::SameStructure
            || Result == ESkeletonCompatibilityResult::Retargetable;
    }
};
