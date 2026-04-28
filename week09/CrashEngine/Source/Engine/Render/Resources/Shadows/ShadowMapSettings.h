#pragma once

#include "Core/CoreTypes.h"

enum class EShadowMapMethod : uint32
{
    Standard = 0,
    PSM = 1,
};

inline EShadowMapMethod GShadowMapMethod = EShadowMapMethod::Standard;

inline EShadowMapMethod GetShadowMapMethod()
{
    return GShadowMapMethod;
}

inline void SetShadowMapMethod(EShadowMapMethod InMethod)
{
    GShadowMapMethod = InMethod;
}

inline const char* GetShadowMapMethodName(EShadowMapMethod InMethod)
{
    switch (InMethod)
    {
    case EShadowMapMethod::Standard:
        return "Standard";
    case EShadowMapMethod::PSM:
        return "PSM";
    default:
        return "Unknown";
    }
}
