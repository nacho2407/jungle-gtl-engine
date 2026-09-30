#pragma once

#include "Core/CoreTypes.h"

enum class EShadowFilterMethod : uint32
{
    None = 0,
    PCF = 1,
    VSM = 2,
    ESM = 3,
};

inline EShadowFilterMethod GShadowFilterMethod = EShadowFilterMethod::None;

inline EShadowFilterMethod GetShadowFilterMethod()
{
    return GShadowFilterMethod;
}

inline void SetShadowFilterMethod(EShadowFilterMethod InMethod)
{
    GShadowFilterMethod = InMethod;
}

inline const char* GetShadowFilterMethodName(EShadowFilterMethod InMethod)
{
    switch (InMethod)
    {
    case EShadowFilterMethod::None:
        return "None (Single Compare)";
    case EShadowFilterMethod::PCF:
        return "PCF (Percentage-Closer Filtering)";
    case EShadowFilterMethod::VSM:
        return "VSM (Variance Shadow Map)";
    case EShadowFilterMethod::ESM:
        return "ESM (Exponential Shadow Map)";
    default:
        return "Unknown";
    }
}

inline const char* GetShadowFilterMethodDefineValue(EShadowFilterMethod InMethod)
{
    switch (InMethod)
    {
    case EShadowFilterMethod::None:
        return "SHADOW_FILTER_METHOD_NONE";
    case EShadowFilterMethod::PCF:
        return "SHADOW_FILTER_METHOD_PCF";
    case EShadowFilterMethod::VSM:
        return "SHADOW_FILTER_METHOD_VSM";
    case EShadowFilterMethod::ESM:
        return "SHADOW_FILTER_METHOD_ESM";
    default:
        return "SHADOW_FILTER_METHOD_NONE";
    }
}
