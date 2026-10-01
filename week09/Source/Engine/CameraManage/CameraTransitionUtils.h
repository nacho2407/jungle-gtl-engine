#pragma once

#include "CameraManage/CameraTypes.h"
#include "Component/CameraComponent.h"
#include "Math/MathUtils.h"

enum class ECameraTransitionBlendType
{
    Linear,
    EaseIn,	//천천히 출발 -> 가속 -> 빠르게 도착
    EaseOut,	//빠르게 출발 -> 감속 -> 부드럽게 도착
    Cubic,	//천천히 출발 -> 중간에서 빠름 -> 천천히 도착
};

struct FCameraTransitionParams
{
    float Duration = 0.0f;
    ECameraTransitionBlendType BlendType = ECameraTransitionBlendType::Cubic;
};

struct FCameraTransitionState
{
    bool bActive = false;
    FCameraViewInfo FromView;
    FCameraViewInfo ToView;
    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    ECameraTransitionBlendType BlendType = ECameraTransitionBlendType::Linear;
};

namespace CameraTransition
{

inline float EvaluateBlendAlpha(float NormalizedTime, ECameraTransitionBlendType BlendType)
{
    const float T = Clamp(NormalizedTime, 0.0f, 1.0f);

    switch (BlendType)
    {
    case ECameraTransitionBlendType::Linear:
        return T;
    case ECameraTransitionBlendType::EaseIn:
        return T * T;
    case ECameraTransitionBlendType::EaseOut:
        return 1.0f - (1.0f - T) * (1.0f - T);
    case ECameraTransitionBlendType::Cubic:
    default:
        return T * T * (3.0f - 2.0f * T);
    }
}

} // namespace CameraTransition