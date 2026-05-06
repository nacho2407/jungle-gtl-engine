#pragma once

#include "Asset/CurveFloatAsset.h"
#include "Core/Containers/String.h"

//  Duration으로 나눈 값을 사용하려면 Normalized Time 사용할 것
enum class ECurveTimeMappingMode : uint8
{
    CurveTime = 0,
    NormalizedTime,
};

//  Curve의 Evaluated된 값을 어떻게 적용할지
enum class ECurveApplyMode : uint8
{
    Direct = 0,
    Add,
    Multiply,
};

struct FCurvePlaybackDesc
{
    FString CurveAssetPath;
    UCurveFloatAsset* Curve = nullptr;

    float StartTime = 0.0f;
    float Duration = 1.0f;
    float PlayRate = 1.0f;

    bool bLoop = false;

    ECurveTimeMappingMode TimeMappingMode = ECurveTimeMappingMode::NormalizedTime;
};

struct FSequenceCurvePlaybackDesc : public FCurvePlaybackDesc
{
    ECurveApplyMode ApplyMode = ECurveApplyMode::Direct;
};

struct FCurvePlaybackEvalResult
{
    bool bActive = false;
    float LocalTime = 0.0f;
    float CurveInputTime = 0.0f;
    float Value = 0.0f;
};

class FCurvePlaybackEvaluator
{
public:
    static FCurvePlaybackEvalResult Evaluate(const FCurvePlaybackDesc& Playback, float SequenceTime);
};
