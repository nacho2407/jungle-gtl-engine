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
