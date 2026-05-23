#pragma once

#include "Core/Types/CoreTypes.h"
#include "Profiling/Stats/Stats.h"

#if STATS
struct FParticleStats
{
	static uint32 SpriteParticleCount;
	static uint32 MeshParticleCount;
	static uint32 DrawCallCount;

	static void Reset()
	{
		SpriteParticleCount = 0;
		MeshParticleCount   = 0;
		DrawCallCount       = 0;
	}
};

#define PARTICLE_STATS_RESET()                     FParticleStats::Reset()
#define PARTICLE_STATS_ADD_SPRITE_PARTICLES(Count) FParticleStats::SpriteParticleCount += (Count)
#define PARTICLE_STATS_ADD_MESH_PARTICLES(Count)   FParticleStats::MeshParticleCount   += (Count)
#define PARTICLE_STATS_ADD_DRAW_CALL()             FParticleStats::DrawCallCount++
#else
#define PARTICLE_STATS_RESET()                     ((void)0)
#define PARTICLE_STATS_ADD_SPRITE_PARTICLES(Count) ((void)0)
#define PARTICLE_STATS_ADD_MESH_PARTICLES(Count)   ((void)0)
#define PARTICLE_STATS_ADD_DRAW_CALL()             ((void)0)
#endif
