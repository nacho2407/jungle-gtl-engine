#include "Animation/AnimSequence.h"

#include "Asset/SkeletalMesh.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(UAnimSequenceBase, UAnimationAsset)

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)
REGISTER_FACTORY(UAnimSequence)

namespace
{
static float NormalizeEvalTime(float Time, float Length, bool bLooping)
{
    if (Length <= 0.0f)
    {
        return 0.0f;
    }

    if (bLooping)
    {
        float WrappedTime = std::fmod(Time, Length);
        if (WrappedTime < 0.0f)
        {
            WrappedTime += Length;
        }
        return WrappedTime;
    }

    return std::clamp(Time, 0.0f, Length);
}
}

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

UAnimSequence::~UAnimSequence()
{
    if (DataModel)
    {
        UObjectManager::Get().DestroyObject(DataModel);
        DataModel = nullptr;
    }
}

float UAnimSequence::GetPlayLength() const
{
    return DataModel ? DataModel->SequenceLength : UAnimSequenceBase::GetPlayLength();
}

bool UAnimSequence::GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const
{
    TArray<FTransform> LocalPose;
    if (!GetAnimationPose(LocalPose, Mesh, FAnimExtractContext(Time, false)))
    {
        return false;
    }

    OutLocalPose.clear();
    OutLocalPose.reserve(LocalPose.size());
    for (const FTransform& BoneTransform : LocalPose)
    {
        OutLocalPose.push_back(BoneTransform.ToMatrixWithScale());
    }

    return true;
}

bool UAnimSequence::GetAnimationPose(
    TArray<FTransform>& OutLocalPose,
    const USkeletalMesh* TargetMesh,
    const FAnimExtractContext& ExtractContext) const
{
    OutLocalPose.clear();

    if (!TargetMesh || !DataModel)
    {
        return false;
    }

    const TArray<FBoneInfo>& Bones = TargetMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0)
    {
        return false;
    }

    const float EvalTime = NormalizeEvalTime(
        ExtractContext.CurrentTime,
        DataModel->SequenceLength,
        ExtractContext.bLooping);

    OutLocalPose.resize(BoneCount);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        OutLocalPose[BoneIndex] = FTransform(Bones[BoneIndex].LocalBindTransform);
    }

    for (const FAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        if (Track.BoneIndex < 0 || Track.BoneIndex >= BoneCount)
        {
            continue;
        }

        const FTransform BindTransform(Bones[Track.BoneIndex].LocalBindTransform);
        const FRawAnimSequenceTrack& Raw = Track.InternalTrackData;

        const FVector Pos = EvalVectorKeys(
            Raw.PosKeys,
            Raw.PosKeyTimes,
            EvalTime,
            BindTransform.GetTranslation());

        const FQuat Rot = EvalQuatKeys(
            Raw.RotKeys,
            Raw.RotKeyTimes,
            EvalTime,
            BindTransform.GetRotation());

        const FVector Scale = EvalVectorKeys(
            Raw.ScaleKeys,
            Raw.ScaleKeyTimes,
            EvalTime,
            BindTransform.GetScale3D());

        OutLocalPose[Track.BoneIndex] = FTransform(Rot, Pos, Scale);
    }

    return true;
}

FVector UAnimSequence::EvalVectorKeys(
    const TArray<FVector>& Keys,
    const TArray<float>& Times,
    float Time,
    const FVector& DefaultValue)
{
    if (Keys.empty())
    {
        return DefaultValue;
    }

    if (Keys.size() == 1 || Times.size() != Keys.size())
    {
        return Keys[0];
    }

    if (Time <= Times.front())
    {
        return Keys.front();
    }

    if (Time >= Times.back())
    {
        return Keys.back();
    }

    for (int32 Index = 0; Index + 1 < static_cast<int32>(Times.size()); ++Index)
    {
        if (Time >= Times[Index] && Time <= Times[Index + 1])
        {
            const float Range = Times[Index + 1] - Times[Index];
            const float Alpha = Range > 1.0e-6f ? (Time - Times[Index]) / Range : 0.0f;
            return Keys[Index] * (1.0f - Alpha) + Keys[Index + 1] * Alpha;
        }
    }

    return Keys.back();
}

FQuat UAnimSequence::EvalQuatKeys(
    const TArray<FQuat>& Keys,
    const TArray<float>& Times,
    float Time,
    const FQuat& DefaultValue)
{
    if (Keys.empty())
    {
        return DefaultValue.GetNormalized();
    }

    if (Keys.size() == 1 || Times.size() != Keys.size())
    {
        return Keys[0].GetNormalized();
    }

    if (Time <= Times.front())
    {
        return Keys.front().GetNormalized();
    }

    if (Time >= Times.back())
    {
        return Keys.back().GetNormalized();
    }

    for (int32 Index = 0; Index + 1 < static_cast<int32>(Times.size()); ++Index)
    {
        if (Time >= Times[Index] && Time <= Times[Index + 1])
        {
            const float Range = Times[Index + 1] - Times[Index];
            const float Alpha = Range > 1.0e-6f ? (Time - Times[Index]) / Range : 0.0f;
            return FQuat::Slerp(Keys[Index], Keys[Index + 1], Alpha).GetNormalized();
        }
    }

    return Keys.back().GetNormalized();
}
