#pragma once
#include "ParticleHelper.h"
#include "Core/Types/CoreTypes.h
#include "ParticleSystem.h"

class UParticleSystemComponent;

struct FBaseParticle
{
	FVector Position;
	FVector OldPosition;
	FVector Velocity;

	FVector Size;
	float Rotation = 0.0f;
	float RotationRate = 0.0f;

	FVector4 Color = { 1, 1, 1, 1 };

	float RelativeTime = 0.0f; // 0~1 normalized age
	float OneOverMaxLifetime = 1.0f;
	float Lifetime = 1.0f;
	float Age = 0.0f;

	uint32 FrameIndex = 0;
	bool bAlive = false;
};

struct FParticleEmitterInstance
{
	UParticleEmitter* SpriteTemplate;

	// Owner
	UParticleSystemComponent* Component;

	int32 CurrentLODLevelIndex;
	UParticleLODLevel* CurrentLODLevel;

	uint8* ParticleData;
	uint16* ParticleIndices;
	uint8* InstanceData;
	int32 InstancePayloadSize;
	int32 PayloadOffset;
	int32 ParticleSize;
	int32 ParticleStride;
	int32 ActiveParticles;
	uint32 ParticleCounter;
	int32 MaxActiveParticles;

	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload)
	{
		for (int32 i = 0; i < Count; i++)
		{
			DECLARE_PARTICLE_PTR
			PreSpawn(Particle, InitialLocation, InitialVelocity);

			for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
			{
				...
			}

			PostSpawn(Particle, Interp, SpawnTime);
		}
	}

	void KillParticle(int32 Index);

};