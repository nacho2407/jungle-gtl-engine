#include "SequenceCameraShakePattern.h"

DEFINE_CLASS(USequenceCameraShakePattern, UCameraShakePattern)
REGISTER_FACTORY(USequenceCameraShakePattern)

// SequenceCameraShakePattern.cpp

void USequenceCameraShakePattern::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UCameraShakePattern::GetEditableProperties(OutProps);

    OutProps.push_back({ "CurveAssetPath", EPropertyType::String, &CurveAssetPath });
    OutProps.push_back({ "PlayRate", EPropertyType::Float, &PlayRate });
    OutProps.push_back({ "Scale", EPropertyType::Float, &Scale });
    OutProps.push_back({ "RandomSegmentDuration", EPropertyType::Float, &RandomSegmentDuration });
    OutProps.push_back({ "bRandomSegment", EPropertyType::Bool, &bRandomSegment });
}

void USequenceCameraShakePattern::OnStartShakePattern(const FCameraShakeStartParams& Params)
{
}

void USequenceCameraShakePattern::OnUpdateShakePattern(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
}
