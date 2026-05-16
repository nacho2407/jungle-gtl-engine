#pragma once

#include "Animation/AnimDataModel.h"
#include "Asset/Skeleton.h"
#include "Object/Object.h"

class USkeletalMesh;

class UAnimSequence : public UObject
{
public:
    DECLARE_CLASS(UAnimSequence, UObject)

    ~UAnimSequence() override;

    FString AssetPath;
    FString SourceFbxPath;
    FString TargetSkeletonPath;

    USkeleton* Skeleton = nullptr;
    UAnimDataModel* DataModel = nullptr;

    bool GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const;

private:
    static FVector EvalVectorKeys(const TArray<FVector>& Keys, const TArray<float>& Times, float Time, const FVector& DefaultValue);

    static FQuat EvalQuatKeys(const TArray<FQuat>& Keys, const TArray<float>& Times, float Time, const FQuat& DefaultValue);
};
