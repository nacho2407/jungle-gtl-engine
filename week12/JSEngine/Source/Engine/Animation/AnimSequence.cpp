#include "Animation/AnimSequence.h"

#include "Object/ObjectFactory.h"
#include "Geometry/Transform.h"

#include <algorithm>
#include <cmath>

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

    NewNotify.TriggerTime = std::clamp(InTriggerTime, 0.0f, GetPlayLength());
    NewNotify.NotifyName = InNotifyName;

    Notifies.push_back(NewNotify);

    std::ranges::sort(Notifies,
            [](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) { return A.TriggerTime < B.TriggerTime; });
}

namespace
{
    int32 GetTrackKeyCount(const FRawAnimSequenceTrack& Track)
    {
        return static_cast<int32>(std::max({
            Track.PosKeys.size(),
            Track.RotKeys.size(),
            Track.ScaleKeys.size()}));
    }

    FVector3f SampleVectorKey(const TArray<FVector3f>& Keys, int32 KeyIndex, int32 NextKeyIndex, float Alpha, const FVector3f& DefaultValue)
    {
        if (Keys.empty())
        {
            return DefaultValue;
        }

        const int32 LastIndex = static_cast<int32>(Keys.size()) - 1;
        const FVector3f& Start = Keys[std::clamp(KeyIndex, 0, LastIndex)];
        const FVector3f& End = Keys[std::clamp(NextKeyIndex, 0, LastIndex)];
        return Start + (End - Start) * Alpha;
    }

    FQuat4f SampleQuatKey(const TArray<FQuat4f>& Keys, int32 KeyIndex, int32 NextKeyIndex, float Alpha)
    {
        if (Keys.empty())
        {
            return FQuat4f::Identity;
        }

        const int32 LastIndex = static_cast<int32>(Keys.size()) - 1;
        const FQuat4f& Start = Keys[std::clamp(KeyIndex, 0, LastIndex)];
        const FQuat4f& End = Keys[std::clamp(NextKeyIndex, 0, LastIndex)];
        return FQuat4f::Slerp(Start, End, Alpha).GetNormalized();
    }
}

float UAnimSequence::GetPlayLength() const
{
    return DataModel ? DataModel->GetPlayLength() : 0.0f;
}

bool UAnimSequence::GetAnimationPose(float Time, FPoseContext& OutPose) const
{
    if (!DataModel)
    {
        return false;
    }

    const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
    if (Tracks.empty())
    {
        return false;
    }

    for (int32 PoseIndex = 0; PoseIndex < static_cast<int32>(OutPose.LocalPose.size()); ++PoseIndex)
    {
        OutPose.LocalPose[PoseIndex] = FMatrix::Identity;
    }

    int32 KeyCount = DataModel->GetNumberOfKeys();
    if (KeyCount <= 0)
    {
        for (const FBoneAnimationTrack& Track : Tracks)
        {
            KeyCount = std::max(KeyCount, GetTrackKeyCount(Track.InternalTrack));
        }
    }

    if (KeyCount <= 0)
    {
        return true;
    }

    const float Length = std::max(0.0f, DataModel->GetPlayLength());
    const float ClampedTime = Length > 0.0f ? std::clamp(Time, 0.0f, Length) : 0.0f;
    const float KeyPosition = (Length > 0.0f && KeyCount > 1)
        ? (ClampedTime / Length) * static_cast<float>(KeyCount - 1)
        : 0.0f;

    const int32 KeyIndex = std::clamp(static_cast<int32>(std::floor(KeyPosition)), 0, KeyCount - 1);
    const int32 NextKeyIndex = std::clamp(KeyIndex + 1, 0, KeyCount - 1);
    const float Alpha = std::clamp(KeyPosition - static_cast<float>(KeyIndex), 0.0f, 1.0f);

    const int32 PoseCount = static_cast<int32>(OutPose.LocalPose.size());
    const int32 TrackCount = static_cast<int32>(Tracks.size());
    const int32 ApplyCount = std::min(PoseCount, TrackCount);
    for (int32 TrackIndex = 0; TrackIndex < ApplyCount; ++TrackIndex)
    {
        const FRawAnimSequenceTrack& RawTrack = Tracks[TrackIndex].InternalTrack;
        const FVector3f Translation = SampleVectorKey(RawTrack.PosKeys, KeyIndex, NextKeyIndex, Alpha, FVector3f::ZeroVector);
        const FQuat4f Rotation = SampleQuatKey(RawTrack.RotKeys, KeyIndex, NextKeyIndex, Alpha);
        const FVector3f Scale = SampleVectorKey(RawTrack.ScaleKeys, KeyIndex, NextKeyIndex, Alpha, FVector3f::OneVector);

        OutPose.LocalPose[TrackIndex] = FTransform(Rotation, Translation, Scale).ToMatrixWithScale();
    }

    return true;
}