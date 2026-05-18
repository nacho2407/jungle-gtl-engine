#include "AnimDataModel.h"

DEFINE_CLASS(UAnimDataModel, UObject)

const FAnimationTrack* UAnimDataModel::FindTrackByBoneIndex(int32 BoneIndex) const
{
    for (const FAnimationTrack& Track : BoneAnimationTracks)
    {
        if (Track.BoneIndex == BoneIndex)
        {
            return &Track;
        }
    }

    return nullptr;
}

const FAnimationTrack* UAnimDataModel::FindTrackByBoneName(const FName& BoneName) const
{
    for (const FAnimationTrack& Track : BoneAnimationTracks)
    {
        if (Track.BoneName == BoneName)
        {
            return &Track;
        }
    }

    return nullptr;
}
