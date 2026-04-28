#pragma once

#include "Core/CoreTypes.h"

enum class EShadowResolution : uint32
{
    R256 = 256u,
    R512 = 512u,
    R1024 = 1024u,
    R2048 = 2048u,
    R4096 = 4096u,
};

inline constexpr EShadowResolution GDefaultShadowResolution = EShadowResolution::R2048;

inline constexpr TStaticArray<EShadowResolution, 5> GShadowResolutionOptions = {
    EShadowResolution::R256,
    EShadowResolution::R512,
    EShadowResolution::R1024,
    EShadowResolution::R2048,
    EShadowResolution::R4096,
};

inline constexpr uint32 GetShadowResolutionValue(EShadowResolution InResolution)
{
    return static_cast<uint32>(InResolution);
}

inline constexpr EShadowResolution RoundShadowResolutionToTier(uint32 RequestedResolution)
{
    if (RequestedResolution <= GetShadowResolutionValue(EShadowResolution::R256))
    {
        return EShadowResolution::R256;
    }
    if (RequestedResolution <= GetShadowResolutionValue(EShadowResolution::R512))
    {
        return EShadowResolution::R512;
    }
    if (RequestedResolution <= GetShadowResolutionValue(EShadowResolution::R1024))
    {
        return EShadowResolution::R1024;
    }
    if (RequestedResolution <= GetShadowResolutionValue(EShadowResolution::R2048))
    {
        return EShadowResolution::R2048;
    }
    return EShadowResolution::R4096;
}

inline constexpr const char* GetShadowResolutionLabel(EShadowResolution InResolution)
{
    switch (InResolution)
    {
    case EShadowResolution::R256:
        return "256";
    case EShadowResolution::R512:
        return "512";
    case EShadowResolution::R1024:
        return "1024";
    case EShadowResolution::R2048:
        return "2048";
    case EShadowResolution::R4096:
        return "4096";
    default:
        return "Unknown";
    }
}
