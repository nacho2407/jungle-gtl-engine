#pragma once

#include "Animation/ActorSequence.h"
#include "Asset/CurveFloatAsset.h"

#include <algorithm>
#include <cmath>

class FEditorActorSequenceTimeUtils
{
public:
    static float GetSafePlayRate(const FActorSequenceSection& Section)
    {
        return std::fabs(Section.PlayRate) > 0.0001f ? Section.PlayRate : 1.0f;
    }

    static float CurveTimeToSequenceTime(
        const FActorSequenceSection& Section,
        const FActorSequenceChannel& Channel,
        float CurveTime)
    {
        const float PlayRate = GetSafePlayRate(Section);
        if (Channel.Playback.TimeMappingMode == ECurveTimeMappingMode::NormalizedTime)
        {
            return Section.StartTime + (CurveTime * Section.Duration) / PlayRate;
        }
        return Section.StartTime + CurveTime / PlayRate;
    }

    static float SequenceTimeToCurveTime(
        const FActorSequenceSection& Section,
        const FActorSequenceChannel& Channel,
        float SequenceTime)
    {
        const float PlayRate = GetSafePlayRate(Section);
        const float LocalTime = (SequenceTime - Section.StartTime) * PlayRate;
        if (Channel.Playback.TimeMappingMode == ECurveTimeMappingMode::NormalizedTime)
        {
            return Section.Duration > 0.0001f ? LocalTime / Section.Duration : 0.0f;
        }
        return LocalTime;
    }

    static void GetDisplaySectionRange(
        const FActorSequenceSection& Section,
        const FActorSequenceChannel& Channel,
        const UCurveFloatAsset* Curve,
        float& OutStartTime,
        float& OutEndTime)
    {
        OutStartTime = Section.StartTime;
        OutEndTime = Section.StartTime + std::max(0.0f, Section.Duration);
        if (!Curve)
        {
            return;
        }

        for (const FCurveKey& Key : Curve->GetCurve().Keys)
        {
            const float KeySequenceTime = CurveTimeToSequenceTime(Section, Channel, Key.Time);
            OutStartTime = std::min(OutStartTime, KeySequenceTime);
            OutEndTime = std::max(OutEndTime, KeySequenceTime);
        }
    }
};
