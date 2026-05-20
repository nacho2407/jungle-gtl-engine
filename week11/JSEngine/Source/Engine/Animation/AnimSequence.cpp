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

float UAnimSequenceBase::GetFrameRate() const
{
    return 0.0f;
}

int32 UAnimSequenceBase::GetNumberOfFrames() const
{
    return 0;
}

void UAnimSequenceBase::SetPlayLength(float InPlayLength)
{
    PlayLength = InPlayLength > 0.0f ? InPlayLength : 0.0f;
}

const TArray<FAnimNotifyEvent>& UAnimSequenceBase::GetNotifies() const
{
    return Notifies;
}

void UAnimSequenceBase::AddNotify(const FAnimNotifyEvent& Notify)
{
    FAnimNotifyEvent SanitizedNotify = Notify;
    if (SanitizedNotify.TriggerTime < 0.0f)
    {
        SanitizedNotify.TriggerTime = 0.0f;
    }
    if (SanitizedNotify.Duration < 0.0f)
    {
        SanitizedNotify.Duration = 0.0f;
    }

    const float SequenceLength = GetPlayLength();
    if (SequenceLength > 0.0f)
    {
        SanitizedNotify.TriggerTime = std::clamp(SanitizedNotify.TriggerTime, 0.0f, SequenceLength);

        // duration notify가 sequence 끝 밖으로 이어지면 등록 시점에 잘라냄
        const float MaxDuration = SequenceLength - SanitizedNotify.TriggerTime;
        if (SanitizedNotify.Duration > MaxDuration)
        {
            SanitizedNotify.Duration = MaxDuration;
        }
    }

    Notifies.push_back(SanitizedNotify);

	// 재생 중 검사 용이, 최적화 가능성을 위한 notify 시간 순 정렬
	// TriggerTime이 동일하면 Duration이 짧은 순으로 정렬(instant notify가 먼저 오도록하는 정책)
    std::sort(
        Notifies.begin(),
        Notifies.end(),
        [](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B)
        {
            if (A.TriggerTime == B.TriggerTime)
            {
                return A.Duration < B.Duration;
            }
            return A.TriggerTime < B.TriggerTime;
        });
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

float UAnimSequence::GetFrameRate() const
{
    return DataModel ? DataModel->FrameRate : UAnimSequenceBase::GetFrameRate();
}

int32 UAnimSequence::GetNumberOfFrames() const
{
    return DataModel ? DataModel->NumberOfFrames : UAnimSequenceBase::GetNumberOfFrames();
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
bool UAnimSequenceBase::RemoveNotifyAt(int32 NotifyIndex)
{
    // std::vector의 size()를 사용하여 유효한 인덱스인지 검사합니다.
    if (NotifyIndex >= 0 && NotifyIndex < static_cast<int32>(Notifies.size()))
    {
        // std::vector의 erase()와 반복자(iterator)를 사용하여 해당 위치의 요소를 삭제합니다.
        Notifies.erase(Notifies.begin() + NotifyIndex);
        return true;
    }

    return false; // 범위를 벗어난 인덱스
}

bool UAnimSequenceBase::UpdateNotifyAt(int32 NotifyIndex, const FAnimNotifyEvent& Notify)
{
    // 유효한 인덱스인지 검사합니다.
    if (NotifyIndex >= 0 && NotifyIndex < static_cast<int32>(Notifies.size()))
    {
        // 배열 연산자를 사용하여 데이터를 덮어씁니다.
        Notifies[NotifyIndex] = Notify;
        return true;
    }

    return false; // 범위를 벗어난 인덱스
}