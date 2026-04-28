#pragma once

#include "Core/CoreTypes.h"

enum class EShadowFilterMethod : uint32
{
    PCF = 0,
    VSM = 1,
    ESM = 2,
};

inline EShadowFilterMethod GShadowFilterMethod = EShadowFilterMethod::PCF;

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
    case EShadowFilterMethod::PCF:
        return "SHADOW_FILTER_METHOD_PCF";
    case EShadowFilterMethod::VSM:
        return "SHADOW_FILTER_METHOD_VSM";
    case EShadowFilterMethod::ESM:
        return "SHADOW_FILTER_METHOD_ESM";
    default:
        return "SHADOW_FILTER_METHOD_PCF";
    }
}
