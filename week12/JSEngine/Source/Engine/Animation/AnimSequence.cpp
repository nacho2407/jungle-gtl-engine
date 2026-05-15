#include "Animation/AnimSequence.h"

#include "Object/ObjectFactory.h"

DEFINE_CLASS(UAnimationAsset, UObject)
DEFINE_CLASS(UAnimDataModel, UObject)
DEFINE_CLASS(UAnimSequenceBase, UAnimationAsset)
DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)

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
