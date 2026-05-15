#include "Animation/AnimSequence.h"

#include "Object/ObjectFactory.h"
#include <algorithm>

DEFINE_CLASS(UAnimationAsset, UObject)
DEFINE_CLASS(UAnimDataModel, UObject)
DEFINE_CLASS(UAnimSequenceBase, UAnimationAsset)
DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)
DEFINE_CLASS(UDebugAnimSequence, UAnimSequenceBase)

REGISTER_FACTORY(UAnimationAsset)
REGISTER_FACTORY(UAnimDataModel)
REGISTER_FACTORY(UAnimSequenceBase)
REGISTER_FACTORY(UAnimSequence)

const TArray<FBoneAnimationTrack>& UAnimDataModel::GetBoneAnimationTracks() const
{
    return BoneAnimationTracks;
}

TArray<FBoneAnimationTrack>& UAnimDataModel::GetMutableBoneAnimationTracks()
{
    return BoneAnimationTracks;
}

void UAnimSequenceBase::AddNotify(float InTriggerTime, const FName& InNotifyName)
{
    FAnimNotifyEvent NewNotify;

    NewNotify.TriggerTime = std::clamp(InTriggerTime, 0.0f, PlayLength);
    NewNotify.NotifyName = InNotifyName;

    Notifies.push_back(NewNotify);

    std::ranges::sort(Notifies,
            [](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) { return A.TriggerTime < B.TriggerTime; });
}

bool UAnimSequence::GetAnimationPose(float Time, FPoseContext& OutPose) const
{
    for (int32 i = 0; i < OutPose.LocalPose.size(); ++i)
    {
        OutPose.LocalPose[i] = FMatrix::Identity;
    }
    return true;
}