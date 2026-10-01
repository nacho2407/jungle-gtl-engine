#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

namespace
{
EShadowAllocationPolicy GShadowAllocationPolicy = EShadowAllocationPolicy::PreferDowngrade;
}

EShadowResolution RoundShadowResolutionToTierPolicy(uint32 RequestedResolution)
{
    return RoundShadowResolutionToTier(RequestedResolution);
}

EShadowAllocationPolicy GetShadowAllocationPolicy()
{
    return GShadowAllocationPolicy;
}

void SetShadowAllocationPolicy(EShadowAllocationPolicy Policy)
{
    GShadowAllocationPolicy = Policy;
}
