#include "AnimSequence.h"

#include "AnimDataModel.h"
#include "PoseContext.h"
#include "AnimExtractContext.h"
#include "Skeleton.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)

namespace
{
    static float NormalizeTime(float Time, float Length, bool bLooping)
    {
        if (Length <= 0.0f)
        {
            return 0.0f;
        }

        if (bLooping)
        {
            float Wrapped = std::fmod(Time, Length);
            if (Wrapped < 0.0f)
            {
                Wrapped += Length;
            }
            return Wrapped;
        }

        return std::clamp(Time, 0.0f, Length);
    }

    template <typename T>
    static const T* GetKeyPtr(const TArray<T>& Keys, int32 Index)
    {
        if (Keys.empty())
        {
            return nullptr;
        }

        const int32 ClampedIndex = std::clamp(
            Index,
            0,
            static_cast<int32>(Keys.size()) - 1);

        return &Keys[ClampedIndex];
    }
}

void UAnimSequence::Serialize(FArchive& Ar)
{
    // 저장 포맷 고정:
    // UObject base + AnimSequence 메타데이터 + UAnimDataModel payload.
    // UAnimSequenceBase::Serialize()는 호출하지 않는다. PlayLength/FrameRate/Notifies는 DataModel의 runtime cache다.
    UObject::Serialize(Ar);

    Ar << AssetPathFileName;
    Ar << SkeletonPath;
    Ar << SkeletonGuid;

    if (!DataModel)
    {
        DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(this);
    }

    DataModel->Serialize(Ar);

    PlayLength = DataModel->PlayLength;
    FrameRate = DataModel->FrameRate;
    Notifies = DataModel->Notifies;
}

void UAnimSequence::SetDataModel(UAnimDataModel* InModel)
{
    DataModel = InModel;

    if (DataModel)
    {
        PlayLength = DataModel->PlayLength;
        FrameRate = DataModel->FrameRate;
        Notifies = DataModel->Notifies;
    }
}

const TArray<FBoneAnimationTrack>& UAnimSequence::GetBoneTracks() const
{
    static const TArray<FBoneAnimationTrack> EmptyTracks;
    return DataModel ? DataModel->BoneAnimationTracks : EmptyTracks;
}

TArray<FBoneAnimationTrack>& UAnimSequence::GetMutableBoneTracks()
{
    static TArray<FBoneAnimationTrack> EmptyTracks;

    if (!DataModel)
    {
        DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(this);
        PlayLength = DataModel->PlayLength;
        FrameRate = DataModel->FrameRate;
        Notifies = DataModel->Notifies;
    }

    return DataModel ? DataModel->BoneAnimationTracks : EmptyTracks;
}

int32 UAnimSequence::GetNumberOfFrames() const
{
    return DataModel ? DataModel->NumFrames : 0;
}

int32 UAnimSequence::TimeToFrame(float TimeSeconds) const
{
    if (!DataModel || DataModel->NumFrames <= 1 || DataModel->FrameRate <= 0.0f)
    {
        return 0;
    }

    const float Time = NormalizeTime(TimeSeconds, DataModel->PlayLength, false);
    const int32 Frame = static_cast<int32>(std::floor(Time * DataModel->FrameRate));

    return std::clamp(Frame, 0, DataModel->NumFrames - 1);
}

float UAnimSequence::FrameToTime(int32 FrameIndex) const
{
    if (!DataModel || DataModel->FrameRate <= 0.0f)
    {
        return 0.0f;
    }

    return static_cast<float>(FrameIndex) / DataModel->FrameRate;
}

void UAnimSequence::GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const
{
    if (!DataModel)
    {
        return;
    }

    if (!Output.SkeletalMesh)
    {
        return;
    }

    FSkeletalMesh* Asset = Output.SkeletalMesh->GetSkeletalMeshAsset();
    if (!Asset)
    {
        return;
    }

    if (Output.Pose.size() != Asset->Bones.size())
    {
        Output.ResetToRefPose();
    }

    const TArray<FBoneAnimationTrack>& Tracks = DataModel->BoneAnimationTracks;
    if (Tracks.empty())
    {
        return;
    }

    const int32 NumFrames = DataModel->NumFrames;
    if (NumFrames <= 0 || DataModel->FrameRate <= 0.0f)
    {
        return;
    }

    const float EvalTime = NormalizeTime(Ctx.CurrentTime, DataModel->PlayLength, Ctx.bLooping);
    const float FrameFloat = EvalTime * DataModel->FrameRate;

    const int32 Frame0 = std::clamp(
        static_cast<int32>(std::floor(FrameFloat)),
        0,
        NumFrames - 1);

    const int32 Frame1 = std::clamp(Frame0 + 1, 0, NumFrames - 1);

    const float Alpha = Frame1 == Frame0
        ? 0.0f
        : std::clamp(FrameFloat - static_cast<float>(Frame0), 0.0f, 1.0f);

    for (const FBoneAnimationTrack& Track : Tracks)
    {
        const int32 BoneIndex = Track.BoneTreeIndex;

        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Output.Pose.size()))
        {
            continue;
        }

        const FRawAnimSequenceTrack& Raw = Track.InternalTrackData;
        FTransform Result = Output.Pose[BoneIndex];

        if (!Raw.PosKeys.empty())
        {
            const FVector* P0 = GetKeyPtr(Raw.PosKeys, Frame0);
            const FVector* P1 = GetKeyPtr(Raw.PosKeys, Frame1);

            if (P0 && P1)
            {
                Result.Location = *P0 + (*P1 - *P0) * Alpha;
            }
        }

        if (!Raw.RotKeys.empty())
        {
            const FQuat* R0 = GetKeyPtr(Raw.RotKeys, Frame0);
            const FQuat* R1 = GetKeyPtr(Raw.RotKeys, Frame1);

            if (R0 && R1)
            {
                Result.Rotation = FQuat::Slerp(
                    R0->GetNormalized(),
                    R1->GetNormalized(),
                    Alpha
                ).GetNormalized();
            }
        }

        if (!Raw.ScaleKeys.empty())
        {
            const FVector* S0 = GetKeyPtr(Raw.ScaleKeys, Frame0);
            const FVector* S1 = GetKeyPtr(Raw.ScaleKeys, Frame1);

            if (S0 && S1)
            {
                Result.Scale = *S0 + (*S1 - *S0) * Alpha;
            }
        }

        Output.Pose[BoneIndex] = Result;
    }
}

bool UAnimSequence::GetAnimationPose(float TimeSeconds, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose) const
{
    OutLocalPose.clear();

    if (!InSkeletalMesh)
    {
        return false;
    }

    FPoseContext Context;
    Context.SkeletalMesh = InSkeletalMesh;
    Context.ResetToRefPose();

    FAnimExtractContext ExtractContext;
    ExtractContext.CurrentTime = TimeSeconds;
    ExtractContext.bLooping = true;
    ExtractContext.bExtractRootMotion = false;

    GetBonePose(Context, ExtractContext);

    OutLocalPose = Context.Pose;
    return !OutLocalPose.empty();
}

bool UAnimSequence::IsCompatibleWith(const USkeleton* InSkeleton) const
{
    if (!InSkeleton)
    {
        return false;
    }

    if (!SkeletonGuid.empty() && !InSkeleton->GetSkeletonGuid().empty())
    {
        return SkeletonGuid == InSkeleton->GetSkeletonGuid();
    }

    return SkeletonPath == InSkeleton->GetAssetPathFileName();
}

bool UAnimSequence::IsCompatibleWith(const USkeletalMesh* InSkeletalMesh) const
{
    if (!InSkeletalMesh)
    {
        return false;
    }

    if (const USkeleton* MeshSkeleton = InSkeletalMesh->GetSkeleton())
    {
        return IsCompatibleWith(MeshSkeleton);
    }

    return SkeletonPath == InSkeletalMesh->GetSkeletonPath();
}
