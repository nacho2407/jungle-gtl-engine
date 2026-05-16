#include "AnimSequence.h"

#include "AnimDataModel.h"
#include "AnimNotify_LogMessage.h"
#include "PoseContext.h"
#include "AnimExtractContext.h"
#include "AnimationRuntime.h"
#include "Skeleton.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"

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

    static float TimeToFrameFloat(float Time, float Length, int32 NumFrames, bool bLooping)
    {
        if (Length <= 0.0f || NumFrames <= 1)
        {
            return 0.0f;
        }

        const float EvalTime = NormalizeTime(Time, Length, bLooping);
        const float Alpha    = std::clamp(EvalTime / Length, 0.0f, 1.0f);

        return Alpha * static_cast<float>(NumFrames - 1);
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
    FrameRate  = DataModel->FrameRate;
    Notifies   = DataModel->Notifies;
}

void UAnimSequence::SetDataModel(UAnimDataModel* InModel)
{
    DataModel = InModel;

    if (DataModel)
    {
        PlayLength = DataModel->PlayLength;
        FrameRate  = DataModel->FrameRate;
        Notifies   = DataModel->Notifies;
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
        FrameRate  = DataModel->FrameRate;
        Notifies   = DataModel->Notifies;
    }

    return DataModel ? DataModel->BoneAnimationTracks : EmptyTracks;
}

int32 UAnimSequence::GetNumberOfFrames() const
{
    return DataModel ? DataModel->NumFrames : 0;
}

int32 UAnimSequence::TimeToFrame(float TimeSeconds) const
{
    if (!DataModel || DataModel->NumFrames <= 0)
    {
        return 0;
    }

    const float FrameFloat = TimeToFrameFloat(TimeSeconds, DataModel->PlayLength, DataModel->NumFrames, false);

    const int32 Frame = static_cast<int32>(std::floor(FrameFloat));
    return std::clamp(Frame, 0, DataModel->NumFrames - 1);
}

float UAnimSequence::FrameToTime(int32 FrameIndex) const
{
    if (!DataModel || DataModel->NumFrames <= 1 || DataModel->PlayLength <= 0.0f)
    {
        return 0.0f;
    }

    const int32 ClampedFrame = std::clamp(FrameIndex, 0, DataModel->NumFrames - 1);

    const float Alpha = static_cast<float>(ClampedFrame) / static_cast<float>(DataModel->NumFrames - 1);

    return Alpha * DataModel->PlayLength;
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
    if (NumFrames <= 0)
    {
        return;
    }

    const float FrameFloat = TimeToFrameFloat(Ctx.CurrentTime, DataModel->PlayLength, NumFrames, Ctx.bLooping);

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

bool UAnimSequence::GetAnimationPose(float TimeSeconds, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose, bool bLooping) const
{
    OutLocalPose.clear();

    if (!InSkeletalMesh)
    {
        return false;
    }

    if (!DataModel || DataModel->BoneAnimationTracks.empty() || DataModel->NumFrames <= 0)
    {
        return false;
    }

    if (!IsCompatibleWith(InSkeletalMesh))
    {
        UE_LOG("Animation pose failed: skeleton mismatch. Anim=%s SkeletonPath=%s", GetName().c_str(), SkeletonPath.c_str());
        return false;
    }

    FPoseContext Context;
    Context.SkeletalMesh = InSkeletalMesh;
    Context.ResetToRefPose();

    FAnimExtractContext ExtractContext;
    ExtractContext.CurrentTime        = TimeSeconds;
    ExtractContext.bLooping           = bLooping;
    ExtractContext.bExtractRootMotion = false;

    GetBonePose(Context, ExtractContext);

    OutLocalPose = Context.Pose;
    return !OutLocalPose.empty();
}

bool UAnimSequence::GetAnimationPoseAtFrame(int32 FrameIndex, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose) const
{
    const float TimeSeconds = FrameToTime(FrameIndex);
    return GetAnimationPose(TimeSeconds, InSkeletalMesh, OutLocalPose, false);
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

const FBoneAnimationTrack* UAnimSequence::FindBoneTrackByIndex(int32 BoneIndex) const
{
    if (!DataModel)
    {
        return nullptr;
    }

    for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        if (Track.BoneTreeIndex == BoneIndex)
        {
            return &Track;
        }
    }
    return nullptr;
}

const FRawAnimSequenceTrack* UAnimSequence::FindTrackByBoneIndex(int32 TrackIndex) const
{
    const FBoneAnimationTrack* Track = FindBoneTrackByIndex(TrackIndex);
    return Track ? &Track->InternalTrackData : nullptr;
}

// ──────────────────────────────────────────────
// Mock factories
// ──────────────────────────────────────────────
UAnimSequence* UAnimSequence::CreateMockSwaySequence(
    USkeletalMesh* InMesh, int32 BoneIdx, float DurationSeconds, float AmplitudeDeg)
{
    if (!InMesh) return nullptr;
    FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset();
    if (!Asset) return nullptr;
    if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(Asset->Bones.size())) return nullptr;

    const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[BoneIdx].LocalMatrix);

    const float Rad   = AmplitudeDeg * FMath::DegToRad;
    const FQuat RotP  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f),  Rad);
    const FQuat RotN  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), -Rad);

    UAnimDataModel* Model = UObjectManager::Get().CreateObject<UAnimDataModel>();
    Model->PlayLength = DurationSeconds;
    Model->FrameRate  = 30.0f;
    Model->NumFrames  = 5;

    FBoneAnimationTrack Track;
    Track.BoneTreeIndex = BoneIdx;
    Track.InternalTrackData.PosKeys   = TArray<FVector>(5, Base.Location);
    Track.InternalTrackData.ScaleKeys = TArray<FVector>(5, Base.Scale);
    Track.InternalTrackData.RotKeys   = {
        Base.Rotation,
        RotP * Base.Rotation,
        Base.Rotation,
        RotN * Base.Rotation,
        Base.Rotation,
    };
    Model->BoneAnimationTracks.push_back(Track);

    UAnimSequence* Seq = UObjectManager::Get().CreateObject<UAnimSequence>();
    Seq->SetDataModel(Model);
    return Seq;
}

UAnimSequence* UAnimSequence::CreateMockWaveSequence(
    USkeletalMesh* InMesh, float DurationSeconds, float AmplitudeDeg)
{
    if (!InMesh) return nullptr;
    FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->Bones.empty()) return nullptr;

    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    const int32 KeyCount  = 9;   // 8 segments, last == first 위상으로 loop-safe
    const float Rad       = AmplitudeDeg * FMath::DegToRad;

    UAnimDataModel* Model = UObjectManager::Get().CreateObject<UAnimDataModel>();
    Model->PlayLength = DurationSeconds;
    Model->FrameRate  = 30.0f;
    Model->NumFrames  = KeyCount;

    for (int32 b = 0; b < BoneCount; ++b)
    {
        const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[b].LocalMatrix);

        // 본 인덱스 별로 위상 차를 줘서 chain 처럼 진행. 한 사이클이 전체 본을 한 바퀴.
        const float PhaseOffset = (static_cast<float>(b) * 2.0f * FMath::Pi)
                                / static_cast<float>(BoneCount);

        FBoneAnimationTrack Track;
        Track.BoneTreeIndex = b;
        Track.InternalTrackData.PosKeys   = TArray<FVector>(KeyCount, Base.Location);
        Track.InternalTrackData.ScaleKeys = TArray<FVector>(KeyCount, Base.Scale);
        Track.InternalTrackData.RotKeys.reserve(KeyCount);

        for (int32 k = 0; k < KeyCount; ++k)
        {
            const float Phase = (static_cast<float>(k) * 2.0f * FMath::Pi)
                              / static_cast<float>(KeyCount - 1) + PhaseOffset;
            const float Angle = Rad * std::sin(Phase);
            const FQuat Rot   = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), Angle);
            Track.InternalTrackData.RotKeys.push_back(Rot * Base.Rotation);
        }

        Model->BoneAnimationTracks.push_back(Track);
    }

    // Phase 7 데모 — wave 시퀀스에 LogMessage notify 2개 박아 dispatch 경로 검증.
    // 트리거는 Duration 의 25% / 75% 지점 — 길이가 짧아도 두 번 모두 발사되는 위치.
    {
        UAnimNotify_LogMessage* N1 = UObjectManager::Get().CreateObject<UAnimNotify_LogMessage>(Model);
        N1->Message = "wave-step (early)";
        Model->Notifies.push_back({ FName("WaveStep"), DurationSeconds * 0.25f, 0.0f, N1 });

        UAnimNotify_LogMessage* N2 = UObjectManager::Get().CreateObject<UAnimNotify_LogMessage>(Model);
        N2->Message = "wave-step (late)";
        Model->Notifies.push_back({ FName("WaveStep"), DurationSeconds * 0.75f, 0.0f, N2 });
    }

    UAnimSequence* Seq = UObjectManager::Get().CreateObject<UAnimSequence>();
    Seq->SetDataModel(Model);
    return Seq;
}
