#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

uint32 RoundShadowResolutionToTier(uint32 RequestedResolution)
{
    if (RequestedResolution <= 256u)
    {
        return 256u;
    }
    if (RequestedResolution <= 512u)
    {
        return 512u;
    }
    if (RequestedResolution <= 1024u)
    {
        return 1024u;
    }
    if (RequestedResolution <= 2048u)
    {
        return 2048u;
    }
    return 4096u;
}
