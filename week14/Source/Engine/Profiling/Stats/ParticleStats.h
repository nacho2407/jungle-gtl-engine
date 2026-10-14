#pragma once

#include "Core/Types/CoreTypes.h"

struct FParticleViewportStats
{
	int32 ParticleSystemCount = 0;
	int32 EmitterCount = 0;
	int32 ActiveEmitterCount = 0;
	int32 ActiveParticles = 0;
	int32 MaxParticles = 0;

	int32 SpriteEmitters = 0;
	int32 MeshEmitters = 0;
	int32 RibbonEmitters = 0;
	int32 BeamEmitters = 0;

	int32 DrawBatches = 0;
	int32 DrawSections = 0;
	int32 MeshInstances = 0;

	uint64 ParticleMemoryBytes = 0;

	void Reset()
	{
		*this = FParticleViewportStats{};
	}

	void Accumulate(const FParticleViewportStats& Other)
	{
		ParticleSystemCount += Other.ParticleSystemCount;
		EmitterCount += Other.EmitterCount;
		ActiveEmitterCount += Other.ActiveEmitterCount;
		ActiveParticles += Other.ActiveParticles;
		MaxParticles += Other.MaxParticles;
		SpriteEmitters += Other.SpriteEmitters;
		MeshEmitters += Other.MeshEmitters;
		RibbonEmitters += Other.RibbonEmitters;
		BeamEmitters += Other.BeamEmitters;
		DrawBatches += Other.DrawBatches;
		DrawSections += Other.DrawSections;
		MeshInstances += Other.MeshInstances;
		ParticleMemoryBytes += Other.ParticleMemoryBytes;
	}
};
