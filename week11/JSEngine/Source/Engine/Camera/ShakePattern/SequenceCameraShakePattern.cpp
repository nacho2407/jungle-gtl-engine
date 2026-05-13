#include "SequenceCameraShakePattern.h"

#include "Animation/CurvePlayback.h"
#include "Core/Logging/Log.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <cstdlib>

DEFINE_CLASS(USequenceCameraShakePattern, UCameraShakePattern)
REGISTER_FACTORY(USequenceCameraShakePattern)

namespace
{
    constexpr int32 CameraShakeCurveChannelCount =
        static_cast<int32>(ECameraShakeCurveChannel::Count);

    constexpr int32 ToChannelIndex(ECameraShakeCurveChannel Channel)
    {
        return static_cast<int32>(Channel);
    }

    float GetPositiveDuration(float InDuration)
    {
        return std::max(InDuration, 0.001f);
    }

    float GetRandomSegmentStart(float Duration, float SegmentDuration)
    {
        const float MaxStart = std::max(Duration - SegmentDuration, 0.0f);
        if (MaxStart <= 0.0f)
        {
            return 0.0f;
        }

        const float RandomAlpha =
            static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return MaxStart * RandomAlpha;
    }

    void ClearCurveValues(float (&Values)[CameraShakeCurveChannelCount])
    {
        std::fill(
            Values,
            Values + CameraShakeCurveChannelCount,
            0.0f);
    }

    void SetAllCurveValues(float (&Values)[CameraShakeCurveChannelCount], float Value)
    {
        std::fill(
            Values,
            Values + CameraShakeCurveChannelCount,
            Value);
    }
}

void USequenceCameraShakePattern::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UCameraShakePattern::GetEditableProperties(OutProps);

    OutProps.push_back({ "CurveAssetPath", EPropertyType::String, &CurveAssetPath });
    OutProps.push_back({ "PlayRate", EPropertyType::Float, &PlayRate });
    OutProps.push_back({ "Scale", EPropertyType::Float, &Scale });
    OutProps.push_back({ "RandomSegmentDuration", EPropertyType::Float, &RandomSegmentDuration });
    OutProps.push_back({ "bRandomSegment", EPropertyType::Bool, &bRandomSegment });
    OutProps.push_back({ "bLoop", EPropertyType::Bool, &bLoop });
    OutProps.push_back({ "LocationAmplitude", EPropertyType::Vec3, &LocationAmplitude });
    OutProps.push_back({ "RotationAmplitudeDeg", EPropertyType::Vec3, &RotationAmplitudeDeg });
    OutProps.push_back({ "FOVAmplitude", EPropertyType::Float, &FOVAmplitude });
}

void USequenceCameraShakePattern::GetCameraShakeInfo(FCameraShakeInfo& OutCameraInfo) const
{
    UCameraShakePattern::GetCameraShakeInfo(OutCameraInfo);

    if (bLoop)
    {
        // Keep the owning camera shake alive while the timeline repeats.
        // Duration still controls one timeline cycle in OnStartShakePattern.
        OutCameraInfo.Duration = 0.0f;
    }
}

void USequenceCameraShakePattern::OnStartShakePattern(const FCameraShakeStartParams& Params)
{
    ClearCurveValues(CurrentCurveValues);
    CameraShakeTimeline.Stop();
    CameraShakeTimeline.ClearTracks();
    CameraShakeTimeline.SetPlayRate(std::max(PlayRate, 0.0f));
    CameraShakeTimeline.SetLoop(bLoop);

    UCurveFloatAsset* PlaybackCurve = Curve;
    if (!PlaybackCurve && !CurveAssetPath.empty())
    {
        PlaybackCurve = FResourceManager::Get().LoadCurve(CurveAssetPath);
        Curve = PlaybackCurve;
    }

    if (!PlaybackCurve)
    {
        UE_LOG_WARNING(
            "[SequenceCameraShakePattern] Curve is not set. Path=%s",
            CurveAssetPath.c_str());
        return;
    }

    FCurvePlaybackDesc Playback;
    Playback.CurveAssetPath = CurveAssetPath;
    Playback.Curve = PlaybackCurve;
    Playback.Duration = GetPositiveDuration(Duration);
    Playback.PlayRate = 1.0f;
    Playback.bLoop = bLoop;
    Playback.TimeMappingMode = ECurveTimeMappingMode::NormalizedTime;

    CameraShakeTimeline.AddFloatTrack(
        "CameraShake",
        Playback,
        [this](float Value)
        {
            SetAllCurveValues(CurrentCurveValues, Value);
        });

    if (bRandomSegment && RandomSegmentDuration > 0.0f)
    {
        CameraShakeTimeline.SetCurrentTime(
            GetRandomSegmentStart(Playback.Duration, RandomSegmentDuration));
    }

    CameraShakeTimeline.Play();
    CameraShakeTimeline.Tick(0.0f);
}

void USequenceCameraShakePattern::OnStopShakePattern(bool bImmediately)
{
    CameraShakeTimeline.Stop();
    ClearCurveValues(CurrentCurveValues);
}

void USequenceCameraShakePattern::OnUpdateShakePattern(
    const FCameraShakeUpdateParams& Params,
    FCameraShakeUpdateResult& OutResult)
{
    ClearCurveValues(CurrentCurveValues);
    CameraShakeTimeline.Tick(Params.DeltaTime);

    OutResult.LocationOffset = FVector(
        LocationAmplitude.X * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::LocationX)] * Scale,
        LocationAmplitude.Y * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::LocationY)] * Scale,
        LocationAmplitude.Z * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::LocationZ)] * Scale);
    OutResult.RotationOffset = FRotator(
        RotationAmplitudeDeg.X * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::Pitch)] * Scale,
        RotationAmplitudeDeg.Y * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::Yaw)] * Scale,
        RotationAmplitudeDeg.Z * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::Roll)] * Scale);
    OutResult.FOVOffset =
        FOVAmplitude * CurrentCurveValues[ToChannelIndex(ECameraShakeCurveChannel::FOV)] * Scale;
}
