#pragma once

#include "Core/CoreTypes.h"
#include "Profiling/Stats.h"

#if STATS
struct FShadowStats
{
	// Shadow map 텍스처 메모리 (bytes)
	static uint64 ShadowMapMemoryBytes;

	// Shadow map 해상도
	static uint32 ShadowMapResolution;

	// 라이트별 shadow caster 개수
	static uint32 SpotLightCasterCount;
	static uint32 PointLightCasterCount;
	static uint32 DirectionalLightCasterCount;

	// Shadow-casting 라이트 개수
	static uint32 SpotLightShadowCount;
	static uint32 PointLightShadowCount;
	static uint32 DirectionalLightShadowCount;

	// Draw call 수
	static uint32 ShadowDrawCallCount;

	static void Reset()
	{
		SpotLightCasterCount = 0;
		PointLightCasterCount = 0;
		DirectionalLightCasterCount = 0;
		SpotLightShadowCount = 0;
		PointLightShadowCount = 0;
		DirectionalLightShadowCount = 0;
		ShadowDrawCallCount = 0;
	}
};

#define SHADOW_STATS_RESET()                     FShadowStats::Reset()
#define SHADOW_STATS_ADD_CASTER(Type, Count)     FShadowStats::Type##CasterCount += (Count)
#define SHADOW_STATS_ADD_DRAW_CALL()             FShadowStats::ShadowDrawCallCount++
#define SHADOW_STATS_SET_MEMORY(Bytes)           FShadowStats::ShadowMapMemoryBytes = (Bytes)
#define SHADOW_STATS_SET_RESOLUTION(Res)         FShadowStats::ShadowMapResolution = (Res)
#define SHADOW_STATS_ADD_SHADOW_LIGHT(Type)      FShadowStats::Type##ShadowCount++
#else
#define SHADOW_STATS_RESET()                     ((void)0)
#define SHADOW_STATS_ADD_CASTER(Type, Count)     ((void)0)
#define SHADOW_STATS_ADD_DRAW_CALL()             ((void)0)
#define SHADOW_STATS_SET_MEMORY(Bytes)           ((void)0)
#define SHADOW_STATS_SET_RESOLUTION(Res)         ((void)0)
#define SHADOW_STATS_ADD_SHADOW_LIGHT(Type)      ((void)0)
#endif
