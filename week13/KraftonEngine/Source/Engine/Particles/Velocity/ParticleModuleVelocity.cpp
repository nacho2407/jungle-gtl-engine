#include "ParticleModuleVelocity.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Math/MathUtils.h"
#include <cstdlib>

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	
	float AlphaX = (float)rand() / (float)RAND_MAX;
	float AlphaY = (float)rand() / (float)RAND_MAX;
	float AlphaZ = (float)rand() / (float)RAND_MAX;

	FVector Vel;
	Vel.X = FMath::Lerp(MinVelocity.X, MaxVelocity.X, AlphaX);
	Vel.Y = FMath::Lerp(MinVelocity.Y, MaxVelocity.Y, AlphaY);
	Vel.Z = FMath::Lerp(MinVelocity.Z, MaxVelocity.Z, AlphaZ);

	if (bApplyOwnerScale)
	{
		Vel *= Context.GetTransform().Scale;
	}
	
	if (bInWorldSpace)
	{
		// Min, Max Velocity 를 World 공간 속도로 해석 -> 이를 Local Space 로 변환해서 Particle 에 누적
		FMatrix WorldToSimulation = Context.Owner.SimulationToWorld.GetInverse();
		Vel = WorldToSimulation.TransformVector(Vel);
	}

	Particle.Velocity += Vel;
	Particle.BaseVelocity += Vel;
}

#if WITH_EDITOR
void UParticleModuleVelocity::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#include "Serialization/Archive.h"

void UParticleModuleVelocity::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	Ar << MinVelocity;
	Ar << MaxVelocity;

	bool bWS = bInWorldSpace;
	bool bOS = bApplyOwnerScale;
	Ar << bWS;
	Ar << bOS;
	if (Ar.IsLoading())
	{
		bInWorldSpace    = bWS ? 1 : 0;
		bApplyOwnerScale = bOS ? 1 : 0;
	}
}
