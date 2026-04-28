#pragma once
#include "Core/CoreTypes.h"

enum class EShadowMapMethod : uint32
{
    SSM = 0,
    PSM = 1,
    CSM = 2,
};

inline EShadowMapMethod GShadowMapMethod = EShadowMapMethod::SSM;

inline EShadowMapMethod GetShadowMapMethod()
{
    return GShadowMapMethod;
}

inline void SetShadowMapMethod(EShadowMapMethod InMethod)
{
    GShadowMapMethod = InMethod;
}

inline const char* GetShadowMapMathodName(EShadowMapMethod InMethod)
{
    switch (InMethod)
    {
    case EShadowMapMethod::SSM:
        return "SSM";
    case EShadowMapMethod::PSM:
        return "PSM";
    case EShadowMapMethod::CSM:
        return "CSM";
    default:
        return "Unknown";
    }
}

// shadow map method 별 hlsl define이 필요하면 define symbol 여기에 추가 (ShadowFilterSettings.h 참고)