#pragma once

#include "Animation/AnimationAsset.h"
#include "Animation/AnimationTypes.h"
#include "Core/Containers/Array.h"
#include "Engine/Geometry/Transform.h"

class USkeletalMesh;

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
    ~UAnimSequence() override = default;

    bool GetAnimationPose(
        TArray<FTransform>& OutLocalPose,
        const USkeletalMesh* TargetMesh,
        const FAnimExtractContext& ExtractContext) const override;
};
