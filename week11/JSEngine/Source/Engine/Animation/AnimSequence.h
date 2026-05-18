#pragma once

#include "Animation/AnimDataModel.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimationTypes.h"
#include "Core/Containers/Array.h"
#include "Engine/Geometry/Transform.h"

class USkeletalMesh;
class USkeleton;

class UAnimSequenceBase : public UAnimationAsset
{
public:
    DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)

    UAnimSequenceBase() = default;
    ~UAnimSequenceBase() override = default;

    float GetPlayLength() const override;
    void SetPlayLength(float InPlayLength);

    virtual bool GetAnimationPose(
        TArray<FTransform>& OutLocalPose,
        const USkeletalMesh* TargetMesh,
        const FAnimExtractContext& ExtractContext) const;

protected:
    float PlayLength = 0.0f;
};

class UAnimSequence : public UAnimSequenceBase
{
public:
    DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

    UAnimSequence() = default;
    ~UAnimSequence() override;

    FString AnimStackName;
    FString AssetPath;
    FString SourceFbxPath;
    FString TargetSkeletonPath;

    USkeleton* Skeleton = nullptr;
    UAnimDataModel* DataModel = nullptr;

    float GetPlayLength() const override;

    bool GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const;

    bool GetAnimationPose(
        TArray<FTransform>& OutLocalPose,
        const USkeletalMesh* TargetMesh,
        const FAnimExtractContext& ExtractContext) const override;

private:
    static FVector EvalVectorKeys(
        const TArray<FVector>& Keys,
        const TArray<float>& Times,
        float Time,
        const FVector& DefaultValue);

    static FQuat EvalQuatKeys(
        const TArray<FQuat>& Keys,
        const TArray<float>& Times,
        float Time,
        const FQuat& DefaultValue);
};
