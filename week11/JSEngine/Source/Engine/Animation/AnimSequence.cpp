#include "Animation/AnimSequence.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(UAnimSequenceBase, UAnimationAsset)

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)
REGISTER_FACTORY(UAnimSequence)

float UAnimSequenceBase::GetPlayLength() const
{
    return PlayLength;
}

void UAnimSequenceBase::SetPlayLength(float InPlayLength)
{
    PlayLength = InPlayLength > 0.0f ? InPlayLength : 0.0f;
}

bool UAnimSequenceBase::GetAnimationPose(
    TArray<FTransform>& OutLocalPose,
    const USkeletalMesh* TargetMesh,
    const FAnimExtractContext& ExtractContext) const
{
    (void)OutLocalPose;
    (void)TargetMesh;
    (void)ExtractContext;
    return false;
}

bool UAnimSequence::GetAnimationPose(
    TArray<FTransform>& OutLocalPose,
    const USkeletalMesh* TargetMesh,
    const FAnimExtractContext& ExtractContext) const
{
    (void)OutLocalPose;
    (void)TargetMesh;
    (void)ExtractContext;
    return false;
}
