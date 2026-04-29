#pragma once

#include "Core/CoreTypes.h"

enum class EShadowMapMethod : uint32
{
    Standard = 0,
    LiPSM = 1,
    Cascade = 2,
};

inline EShadowMapMethod GShadowMapMethod = EShadowMapMethod::Cascade;

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
    case EShadowMapMethod::LiPSM:
        return "LiPSM";
    case EShadowMapMethod::Cascade:
        return "Cascade";
    default:
        return "Unknown";
    }
}
