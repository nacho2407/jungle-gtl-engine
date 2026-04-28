#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

EShadowResolution RoundShadowResolutionToTierPolicy(uint32 RequestedResolution)
{
    return RoundShadowResolutionToTier(RequestedResolution);
}
