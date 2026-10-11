#include "ShadowStats.h"

#if STATS
uint64 FShadowStats::ShadowMapMemoryBytes = 0;
uint32 FShadowStats::ShadowMapResolution = 0;
uint32 FShadowStats::SpotLightCasterCount = 0;
uint32 FShadowStats::PointLightCasterCount = 0;
uint32 FShadowStats::DirectionalLightCasterCount = 0;
uint32 FShadowStats::SpotLightShadowCount = 0;
uint32 FShadowStats::PointLightShadowCount = 0;
uint32 FShadowStats::DirectionalLightShadowCount = 0;
uint32 FShadowStats::ShadowDrawCallCount = 0;
#endif
