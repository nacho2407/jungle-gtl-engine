#include "AnimSequence.h"

#include "AnimDataModel.h"
#include "AnimNotify_LogMessage.h"
#include "PoseContext.h"
#include "AnimExtractContext.h"
#include "AnimationRuntime.h"
#include "Skeleton.h"
#include "SkeletonManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Math/MathUtils.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>
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

    // FBX SDK의 EInterpolationType 값은 bit flag 형태(상수=1, 선형=2, 큐빅=4)로 저장되어 있다.
    // AnimSequence 런타임에서는 FBX SDK 헤더에 의존하지 않도록 숫자만 해석한다.
    static constexpr int32 RawCurveInterpConstant = 1;
    static constexpr int32 RawCurveInterpLinear   = 2;
    static constexpr int32 RawCurveInterpCubic    = 4;

    static int32 GetRawCurveInterpolationType(int32 RawInterpolation)
    {
        if ((RawInterpolation & RawCurveInterpCubic) == RawCurveInterpCubic)
        {
            return RawCurveInterpCubic;
        }

        if ((RawInterpolation & RawCurveInterpLinear) == RawCurveInterpLinear)
        {
            return RawCurveInterpLinear;
        }

        if ((RawInterpolation & RawCurveInterpConstant) == RawCurveInterpConstant)
        {
            return RawCurveInterpConstant;
        }

        return RawCurveInterpLinear;
    }

    static bool HasRawCurveKeys(const FRawFloatCurve& Curve)
    {
        return !Curve.Keys.empty();
    }

    static bool HasSourceCurveKeys(const FRawAnimSequenceTrack& Raw)
    {
        for (const FSourceTransformCurveLayer& Layer : Raw.SourceCurveLayers)
        {
            if (Layer.HasAnyKeys())
            {
                return true;
            }
        }
        return false;
    }


    static bool HasAnySourceCurveKeys(const TArray<FBoneAnimationTrack>& Tracks)
    {
        for (const FBoneAnimationTrack& Track : Tracks)
        {
            if (HasSourceCurveKeys(Track.InternalTrackData))
            {
                return true;
            }
        }
        return false;
    }

    static float EstimateRawCurveTangent(const FRawFloatCurve& Curve, int32 KeyIndex)
    {
        const int32 NumKeys = static_cast<int32>(Curve.Keys.size());
        if (NumKeys <= 1 || KeyIndex < 0 || KeyIndex >= NumKeys)
        {
            return 0.0f;
        }

        const int32 PrevIndex = std::max(0, KeyIndex - 1);
        const int32 NextIndex = std::min(NumKeys - 1, KeyIndex + 1);

        const FRawFloatCurveKey& Prev = Curve.Keys[PrevIndex];
        const FRawFloatCurveKey& Next = Curve.Keys[NextIndex];

        const float DeltaTime = Next.TimeSeconds - Prev.TimeSeconds;
        if (std::abs(DeltaTime) < 1.0e-6f)
        {
            return 0.0f;
        }

        return (Next.Value - Prev.Value) / DeltaTime;
    }

    static float CubicBezier(float A, float B, float C, float D, float T)
    {
        const float U = 1.0f - T;
        return U * U * U * A + 3.0f * U * U * T * B + 3.0f * U * T * T * C + T * T * T * D;
    }

    static float CubicBezierDerivative(float A, float B, float C, float D, float T)
    {
        const float U = 1.0f - T;
        return 3.0f * U * U * (B - A) + 6.0f * U * T * (C - B) + 3.0f * T * T * (D - C);
    }

    static float SolveBezierTime(float TargetTime, float X0, float X1, float X2, float X3)
    {
        float       T     = 0.5f;
        const float Denom = X3 - X0;
        if (std::abs(Denom) > 1.0e-6f)
        {
            T = std::clamp((TargetTime - X0) / Denom, 0.0f, 1.0f);
        }

        for (int32 Iter = 0; Iter < 8; ++Iter)
        {
            const float X  = CubicBezier(X0, X1, X2, X3, T);
            const float DX = CubicBezierDerivative(X0, X1, X2, X3, T);
            if (std::abs(DX) < 1.0e-6f)
            {
                break;
            }
            T = std::clamp(T - (X - TargetTime) / DX, 0.0f, 1.0f);
        }
        return T;
    }

    static float EvaluateRawCurveSegment(const FRawFloatCurve& Curve, int32 KeyIndex, float TimeSeconds)
    {
        const FRawFloatCurveKey& A = Curve.Keys[KeyIndex];
        const FRawFloatCurveKey& B = Curve.Keys[KeyIndex + 1];

        const float DeltaTime = B.TimeSeconds - A.TimeSeconds;
        if (std::abs(DeltaTime) < 1.0e-6f)
        {
            return B.Value;
        }

        const float Alpha = std::clamp((TimeSeconds - A.TimeSeconds) / DeltaTime, 0.0f, 1.0f);

        switch (GetRawCurveInterpolationType(A.Interpolation))
        {
        case RawCurveInterpConstant:
            return A.Value;

        case RawCurveInterpCubic:
        {
            float LeaveWeight  = A.bLeaveTangentWeighted ? A.LeaveTangentWeight : DeltaTime / 3.0f;
            float ArriveWeight = B.bArriveTangentWeighted ? B.ArriveTangentWeight : DeltaTime / 3.0f;
            LeaveWeight        = std::clamp(LeaveWeight, 1.0e-5f, DeltaTime);
            ArriveWeight       = std::clamp(ArriveWeight, 1.0e-5f, DeltaTime);

            float LeaveTangent  = A.LeaveTangent;
            float ArriveTangent = B.ArriveTangent;
            if (A.TangentMode == 0)
            {
                LeaveTangent = EstimateRawCurveTangent(Curve, KeyIndex);
            }
            if (B.TangentMode == 0)
            {
                ArriveTangent = EstimateRawCurveTangent(Curve, KeyIndex + 1);
            }

            const float X0 = A.TimeSeconds;
            const float Y0 = A.Value;
            const float X1 = A.TimeSeconds + LeaveWeight;
            const float Y1 = A.Value + LeaveTangent * LeaveWeight;
            const float X2 = B.TimeSeconds - ArriveWeight;
            const float Y2 = B.Value - ArriveTangent * ArriveWeight;
            const float X3 = B.TimeSeconds;
            const float Y3 = B.Value;

            const float BezierT = SolveBezierTime(TimeSeconds, X0, X1, X2, X3);
            return CubicBezier(Y0, Y1, Y2, Y3, BezierT);
        }

        case RawCurveInterpLinear:
        default:
            return A.Value + (B.Value - A.Value) * Alpha;
        }
    }

    static float EvaluateRawFloatCurve(const FRawFloatCurve& Curve, float TimeSeconds, float DefaultValue)
    {
        const int32 NumKeys = static_cast<int32>(Curve.Keys.size());
        if (NumKeys <= 0)
        {
            return DefaultValue;
        }

        if (NumKeys == 1)
        {
            return Curve.Keys[0].Value;
        }

        if (TimeSeconds <= Curve.Keys.front().TimeSeconds)
        {
            return Curve.Keys.front().Value;
        }

        if (TimeSeconds >= Curve.Keys.back().TimeSeconds)
        {
            return Curve.Keys.back().Value;
        }

        for (int32 KeyIndex = 0; KeyIndex + 1 < NumKeys; ++KeyIndex)
        {
            const FRawFloatCurveKey& A = Curve.Keys[KeyIndex];
            const FRawFloatCurveKey& B = Curve.Keys[KeyIndex + 1];

            if (TimeSeconds >= A.TimeSeconds && TimeSeconds <= B.TimeSeconds)
            {
                return EvaluateRawCurveSegment(Curve, KeyIndex, TimeSeconds);
            }
        }

        return Curve.Keys.back().Value;
    }

    static bool EvaluateRawVectorCurve(const FRawVectorCurve& Curve, float TimeSeconds, const FVector& DefaultValue, FVector& OutValue)
    {
        const bool bHasX = HasRawCurveKeys(Curve.X);
        const bool bHasY = HasRawCurveKeys(Curve.Y);
        const bool bHasZ = HasRawCurveKeys(Curve.Z);

        if (!bHasX && !bHasY && !bHasZ)
        {
            OutValue = DefaultValue;
            return false;
        }

        OutValue.X = bHasX ? EvaluateRawFloatCurve(Curve.X, TimeSeconds, DefaultValue.X) : DefaultValue.X;
        OutValue.Y = bHasY ? EvaluateRawFloatCurve(Curve.Y, TimeSeconds, DefaultValue.Y) : DefaultValue.Y;
        OutValue.Z = bHasZ ? EvaluateRawFloatCurve(Curve.Z, TimeSeconds, DefaultValue.Z) : DefaultValue.Z;
        return true;
    }

    static bool HasSoloSourceLayer(const FRawAnimSequenceTrack& Raw)
    {
        for (const FSourceTransformCurveLayer& Layer : Raw.SourceCurveLayers)
        {
            if (Layer.bSolo && Layer.HasAnyKeys())
            {
                return true;
            }
        }
        return false;
    }

    static bool ShouldUseSourceLayer(const FSourceTransformCurveLayer& Layer, bool bHasSoloLayer)
    {
        if (!Layer.HasAnyKeys())
        {
            return false;
        }

        if (bHasSoloLayer)
        {
            return Layer.bSolo;
        }

        return !Layer.bMute;
    }

    static float GetClampedSourceLayerWeight(const FSourceTransformCurveLayer& Layer)
    {
        return std::clamp(Layer.LayerWeight, 0.0f, 1.0f);
    }

    static FTransform EvaluateSourceCurveTrack(const FRawAnimSequenceTrack& Raw, float TimeSeconds, const FTransform& FallbackTransform)
    {
        FTransform Result = FallbackTransform;

        if (Raw.SourceCurveLayers.empty())
        {
            return Result;
        }

        const bool bHasSoloLayer = HasSoloSourceLayer(Raw);

        for (const FSourceTransformCurveLayer& Layer : Raw.SourceCurveLayers)
        {
            if (!ShouldUseSourceLayer(Layer, bHasSoloLayer))
            {
                continue;
            }

            const float Weight = GetClampedSourceLayerWeight(Layer);
            if (Weight <= 0.0f)
            {
                continue;
            }

            FVector CurveLocation;
            if (EvaluateRawVectorCurve(Layer.Translation, TimeSeconds, Result.Location, CurveLocation))
            {
                Result.Location = FVector::Lerp(Result.Location, CurveLocation, Weight);
            }

            FVector CurveRotationEuler;
            const FVector CurrentEuler = Result.Rotation.ToRotator().ToVector();
            if (EvaluateRawVectorCurve(Layer.Rotation, TimeSeconds, CurrentEuler, CurveRotationEuler))
            {
                const FQuat CurveRotation = FRotator(CurveRotationEuler).ToQuaternion().GetNormalized();
                Result.Rotation = FQuat::Slerp(Result.Rotation.GetNormalized(), CurveRotation, Weight).GetNormalized();
            }

            FVector CurveScale;
            if (EvaluateRawVectorCurve(Layer.Scale, TimeSeconds, Result.Scale, CurveScale))
            {
                Result.Scale = FVector::Lerp(Result.Scale, CurveScale, Weight);
            }
        }

        return Result;
    }
}

void UAnimSequence::Serialize(FArchive& Ar)
{
    // 저장 포맷 고정:
    // UObject base + AnimSequence 메타데이터 + UAnimDataModel payload.
    // UAnimSequenceBase::Serialize()는 호출하지 않는다. PlayLength/FrameRate/Notifies는 DataModel의 runtime cache다.
    UObject::Serialize(Ar);

    Ar << AssetPathFileName;
    Ar << TargetSkeleton;
    Ar << bForceRootLock;
    Ar << bEnableRootMotion;
    Ar << RootMotionBoneName;

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

const TArray<FMorphTargetCurve>& UAnimSequence::GetMorphTargetCurves() const
{
    static const TArray<FMorphTargetCurve> EmptyCurves;
    return DataModel ? DataModel->MorphTargetCurves : EmptyCurves;
}

TArray<FMorphTargetCurve>& UAnimSequence::GetMutableMorphTargetCurves()
{
    static TArray<FMorphTargetCurve> EmptyCurves;

    if (!DataModel)
    {
        DataModel  = UObjectManager::Get().CreateObject<UAnimDataModel>(this);
        PlayLength = DataModel->PlayLength;
        FrameRate  = DataModel->FrameRate;
        Notifies   = DataModel->Notifies;
    }

    return DataModel ? DataModel->MorphTargetCurves : EmptyCurves;
}

void UAnimSequence::EvaluateMorphTargetCurves(
    float          TimeSeconds,
    bool           bLooping,
    USkeletalMesh* InSkeletalMesh,
    TArray<float>& OutWeights
    ) const
{
    OutWeights.clear();

    if (!DataModel || !InSkeletalMesh)
    {
        return;
    }

    FSkeletalMesh* Asset = InSkeletalMesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->MorphTargets.empty())
    {
        return;
    }

    OutWeights.assign(Asset->MorphTargets.size(), 0.0f);
    const float EvalTime = NormalizeTime(TimeSeconds, DataModel->PlayLength, bLooping);

    for (const FMorphTargetCurve& MorphCurve : DataModel->MorphTargetCurves)
    {
        if (!MorphCurve.bEnabled || MorphCurve.MorphTargetName.empty())
        {
            continue;
        }

        const int32 MorphIndex = Asset->FindMorphTargetIndex(MorphCurve.MorphTargetName);
        if (MorphIndex < 0 || MorphIndex >= static_cast<int32>(OutWeights.size()))
        {
            continue;
        }

        const float CurveValue = EvaluateRawFloatCurve(MorphCurve.Curve, EvalTime, 0.0f);
        OutWeights[MorphIndex] = CurveValue * MorphCurve.WeightScale + MorphCurve.WeightBias;
    }
}

TArray<FAnimNotifyEvent>& UAnimSequence::GetMutableModelNotifies()
{
    if (!DataModel)
    {
        DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(this);
        PlayLength = DataModel->PlayLength;
        FrameRate  = DataModel->FrameRate;
        Notifies   = DataModel->Notifies;
    }
    return DataModel->Notifies;
}

void UAnimSequence::RefreshRuntimeNotifies()
{
    if (DataModel)
    {
        // 베이스 캐시 = UAnimInstance::AddAnimNotifies 가 읽는 dispatch 소스.
        Notifies = DataModel->Notifies;
    }
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

    if (!IsCompatibleWith(Output.SkeletalMesh))
    {
        UE_LOG("Animation pose rejected: skeleton mismatch. Anim=%s SkeletonPath=%s", GetName().c_str(), TargetSkeleton.SkeletonPath.c_str());
        Output.ResetToRefPose();
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

    const float EvalTime = NormalizeTime(Ctx.CurrentTime, DataModel->PlayLength, Ctx.bLooping);
    EvaluateMorphTargetCurves(EvalTime, Ctx.bLooping, Output.SkeletalMesh, Output.MorphWeights);

    const TArray<FBoneAnimationTrack>& Tracks = DataModel->BoneAnimationTracks;
    if (Tracks.empty())
    {
        return;
    }

    const int32 NumFrames = DataModel->NumFrames;
    const bool  bCanEvaluateSourceCurves = HasAnySourceCurveKeys(Tracks);

    if (NumFrames <= 0 && !bCanEvaluateSourceCurves)
    {
        return;
    }

    int32 Frame0 = 0;
    int32 Frame1 = 0;
    float Alpha  = 0.0f;

    if (NumFrames > 0)
    {
        const float FrameFloat = TimeToFrameFloat(Ctx.CurrentTime, DataModel->PlayLength, NumFrames, Ctx.bLooping);

        Frame0 = std::clamp(
            static_cast<int32>(std::floor(FrameFloat)),
            0,
            NumFrames - 1);

        Frame1 = std::clamp(Frame0 + 1, 0, NumFrames - 1);

        Alpha = Frame1 == Frame0
            ? 0.0f
            : std::clamp(FrameFloat - static_cast<float>(Frame0), 0.0f, 1.0f);
    }

    for (const FBoneAnimationTrack& Track : Tracks)
    {
        int32 BoneIndex = Track.BoneTreeIndex;

        if (!Track.BoneName.empty())
        {
            const bool bIndexInvalid = BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size());
            const bool bIndexNameMismatch = !bIndexInvalid && Asset->Bones[BoneIndex].Name != Track.BoneName;
            if (bIndexInvalid || bIndexNameMismatch)
            {
                if (const USkeleton* MeshSkeleton = Output.SkeletalMesh->GetSkeleton())
                {
                    BoneIndex = MeshSkeleton->FindBoneIndex(Track.BoneName);
                }
                else
                {
                    BoneIndex = -1;
                    for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(Asset->Bones.size()); ++CandidateIndex)
                    {
                        if (Asset->Bones[CandidateIndex].Name == Track.BoneName)
                        {
                            BoneIndex = CandidateIndex;
                            break;
                        }
                    }
                }
            }
        }

        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Output.Pose.size()))
        {
            continue;
        }

        const FRawAnimSequenceTrack& Raw = Track.InternalTrackData;
        FTransform Result = Output.Pose[BoneIndex];
        const FVector BindLocation = Result.Location;   // pre-anim local (ResetToRefPose 결과)

        // Root 본 motion 의 본 pose 적용 제어:
        //   - Force Root Lock: horizontal (X/Y) translation 만 bind 로 고정 (Z 는 anim 유지).
        //   - Enable Root Motion: 전체 translation (X/Y/Z) 을 bind 로 고정 → actor 가 delta 받아 이동.
        //   둘 다 켜진 경우는 SetEnableRootMotion 에서 Force Root Lock 자동 해제됨.
        const bool bIsRootMotionBone =
            !RootMotionBoneName.empty() &&
            !Track.BoneName.empty() &&
            Track.BoneName == RootMotionBoneName;
        const bool bSuppressHorizontalTranslation = bIsRootMotionBone && bForceRootLock;
        const bool bSuppressAllTranslation        = bIsRootMotionBone && bEnableRootMotion;

        if (!Raw.PosKeys.empty())
        {
            const FVector* P0 = GetKeyPtr(Raw.PosKeys, Frame0);
            const FVector* P1 = GetKeyPtr(Raw.PosKeys, Frame1);

            if (P0 && P1)
            {
                Result.Location = *P0 + (*P1 - *P0) * Alpha;
                if (bSuppressAllTranslation)
                {
                    // Root Motion: 전체 translation 을 bind 로 → 본은 정지, actor 가 delta 로 이동.
                    Result.Location = BindLocation;
                }
                else if (bSuppressHorizontalTranslation)
                {
                    // Force Root Lock: X/Y bind, Z anim → in-place 걷기 + bobbing.
                    Result.Location.X = BindLocation.X;
                    Result.Location.Y = BindLocation.Y;
                }
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

    if (!DataModel || DataModel->BoneAnimationTracks.empty())
    {
        return false;
    }

    if (DataModel->NumFrames <= 0 && !HasAnySourceCurveKeys(DataModel->BoneAnimationTracks))
    {
        return false;
    }

    if (!IsCompatibleWith(InSkeletalMesh))
    {
        UE_LOG("Animation pose failed: skeleton mismatch. Anim=%s SkeletonPath=%s", GetName().c_str(), TargetSkeleton.SkeletonPath.c_str());
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

    const FSkeletonCompatibilityReport Report = FSkeletonManager::CheckCompatibility(
        TargetSkeleton,
        InSkeleton->GetSkeletonBinding(),
        nullptr,
        InSkeleton);

    return Report.IsCompatible(false);
}

bool UAnimSequence::IsCompatibleWith(const USkeletalMesh* InSkeletalMesh) const
{
    if (!InSkeletalMesh)
    {
        return false;
    }

    const USkeleton* MeshSkeleton = InSkeletalMesh->GetSkeleton();
    const FSkeletonCompatibilityReport Report = FSkeletonManager::CheckCompatibility(
        TargetSkeleton,
        InSkeletalMesh->GetSkeletonBinding(),
        nullptr,
        MeshSkeleton);

    return Report.IsCompatible(false);
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

namespace
{
    // Track 에서 시간 T 의 (Pos, Rot) 보간값을 추출. PosKeys/RotKeys 없으면 (0, Identity).
    static void SampleTrackPosRot(
        const FRawAnimSequenceTrack& Raw,
        float TimeSeconds, float PlayLength, int32 NumFrames,
        FVector& OutPos, FQuat& OutRot)
    {
        OutPos = FVector::ZeroVector;
        OutRot = FQuat::Identity;
        if (NumFrames <= 0 || PlayLength <= 0.0f) return;

        const float FrameFloat = std::clamp(TimeSeconds / PlayLength, 0.0f, 1.0f) * static_cast<float>(NumFrames - 1);
        const int32 F0 = std::clamp(static_cast<int32>(std::floor(FrameFloat)), 0, NumFrames - 1);
        const int32 F1 = std::clamp(F0 + 1, 0, NumFrames - 1);
        const float Alpha = (F1 == F0) ? 0.0f : std::clamp(FrameFloat - static_cast<float>(F0), 0.0f, 1.0f);

        if (!Raw.PosKeys.empty())
        {
            const FVector& P0 = Raw.PosKeys[std::clamp(F0, 0, (int32)Raw.PosKeys.size() - 1)];
            const FVector& P1 = Raw.PosKeys[std::clamp(F1, 0, (int32)Raw.PosKeys.size() - 1)];
            OutPos = P0 + (P1 - P0) * Alpha;
        }
        if (!Raw.RotKeys.empty())
        {
            const FQuat& R0 = Raw.RotKeys[std::clamp(F0, 0, (int32)Raw.RotKeys.size() - 1)];
            const FQuat& R1 = Raw.RotKeys[std::clamp(F1, 0, (int32)Raw.RotKeys.size() - 1)];
            OutRot = FQuat::Slerp(R0.GetNormalized(), R1.GetNormalized(), Alpha).GetNormalized();
        }
    }
}

FTransform UAnimSequence::ExtractRootMotion(float PrevTime, float CurTime, bool bLoop) const
{
    FTransform Delta;  // Identity 기본

    if (!DataModel || RootMotionBoneName.empty()) return Delta;
    const float Length = DataModel->PlayLength;
    const int32 NumFrames = DataModel->NumFrames;
    if (Length <= 0.0f || NumFrames <= 0) return Delta;

    // RootMotionBoneName 본의 track 찾기.
    const FRawAnimSequenceTrack* Raw = nullptr;
    for (const FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        if (Track.BoneName == RootMotionBoneName)
        {
            Raw = &Track.InternalTrackData;
            break;
        }
    }
    if (!Raw) return Delta;

    auto SampleAt = [&](float T, FVector& OutP, FQuat& OutR) {
        SampleTrackPosRot(*Raw, std::clamp(T, 0.0f, Length), Length, NumFrames, OutP, OutR);
    };

    auto ComputeDelta = [](const FVector& P0, const FQuat& R0, const FVector& P1, const FQuat& R1) -> FTransform {
        FTransform D;
        D.Location = P1 - P0;
        D.Rotation = (R1 * R0.Inverse()).GetNormalized();
        return D;
    };

    // Loop wrap 처리 — 시간이 [Length-x, ε] 처럼 끝에서 0 으로 감기면 두 구간 합산.
    if (bLoop && CurTime < PrevTime)
    {
        FVector PA, PB; FQuat RA, RB;
        // [PrevTime, Length]
        SampleAt(PrevTime, PA, RA);
        SampleAt(Length,   PB, RB);
        const FTransform D1 = ComputeDelta(PA, RA, PB, RB);
        // [0, CurTime]
        SampleAt(0.0f,   PA, RA);
        SampleAt(CurTime, PB, RB);
        const FTransform D2 = ComputeDelta(PA, RA, PB, RB);
        // 합산: D2 ∘ D1 — Position 단순 합, Rotation 곱.
        Delta.Location = D1.Location + D2.Location;
        Delta.Rotation = (D2.Rotation * D1.Rotation).GetNormalized();
        return Delta;
    }

    FVector P0, P1; FQuat R0, R1;
    SampleAt(PrevTime, P0, R0);
    SampleAt(CurTime,  P1, R1);
    return ComputeDelta(P0, R0, P1, R1);
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

    const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[BoneIdx].GetReferenceLocalPose());

    const float Rad   = AmplitudeDeg * FMath::DegToRad;
    const FQuat RotP  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f),  Rad);
    const FQuat RotN  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), -Rad);

    UAnimDataModel* Model = UObjectManager::Get().CreateObject<UAnimDataModel>();
    Model->PlayLength = DurationSeconds;
    Model->FrameRate  = 30.0f;
    Model->NumFrames  = 5;

    FBoneAnimationTrack Track;
    Track.BoneTreeIndex = BoneIdx;
    Track.BoneName = Asset->Bones[BoneIdx].Name;
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
    Seq->SetSkeletonBinding(InMesh->GetSkeletonBinding());
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
        const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[b].GetReferenceLocalPose());

        // 본 인덱스 별로 위상 차를 줘서 chain 처럼 진행. 한 사이클이 전체 본을 한 바퀴.
        const float PhaseOffset = (static_cast<float>(b) * 2.0f * FMath::Pi)
                                / static_cast<float>(BoneCount);

        FBoneAnimationTrack Track;
        Track.BoneTreeIndex = b;
        Track.BoneName = Asset->Bones[b].Name;
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
    Seq->SetSkeletonBinding(InMesh->GetSkeletonBinding());
    return Seq;
}
