#include "Animation/CurvePlayback.h"

#include <cmath>

FCurvePlaybackEvalResult FCurvePlaybackEvaluator::Evaluate(const FCurvePlaybackDesc& Playback, float SequenceTime)
{
    FCurvePlaybackEvalResult Result;

    if (!Playback.Curve)
    {
        return Result;
    }

    if (SequenceTime < Playback.StartTime)
    {
        return Result;
    }

    float LocalTime = (SequenceTime - Playback.StartTime) * Playback.PlayRate;
    if (!Playback.bLoop && LocalTime > Playback.Duration)
    {
        return Result;
    }

    if (Playback.bLoop && Playback.Duration > 0.0f)
    {
        LocalTime = std::fmod(LocalTime, Playback.Duration);
    }

    float CurveInputTime = LocalTime;
    if (Playback.TimeMappingMode == ECurveTimeMappingMode::NormalizedTime)
    {
        CurveInputTime = Playback.Duration > 0.0f ? LocalTime / Playback.Duration : 0.0f;
    }

    Result.bActive = true;
    Result.LocalTime = LocalTime;
    Result.CurveInputTime = CurveInputTime;
    Result.Value = Playback.Curve->Evaluate(CurveInputTime);
    return Result;
}
