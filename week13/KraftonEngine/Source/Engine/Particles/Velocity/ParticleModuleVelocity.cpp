#include "ParticleModuleVelocity.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Math/MathUtils.h"
#include <cstdlib>

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	
	FVector Vel = StartVelocity.GetValue(Context.Owner.EmitterTime, Context.Owner.Component);

	FVector FromOrigin = (Particle.Location - Context.Owner.EmitterToSimulation.GetOrigin()).GetSafeNormal();

	FVector OwnerScale = FVector::OneVector;
	if (bApplyOwnerScale == true)
	{
		OwnerScale = Context.GetTransform().Scale;
	}

	UParticleLODLevel* LODLevel = Context.Owner.GetCurrentLODLevelChecked();
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		if (bInWorldSpace == true)
		{
			// Min, Max Velocity 를 World 공간 속도로 해석 -> 이를 Local Space 로 변환해서 Particle 에 누적
			FMatrix WorldToSimulation = Context.Owner.SimulationToWorld.GetInverse();
			Vel = WorldToSimulation.TransformVector(Vel);
		}
		else
		{
			// Emitter Space != Simulation Space 가정
			Vel = Context.Owner.EmitterToSimulation.TransformVector(Vel);
		}
	}
	else if (bInWorldSpace == false)
	{
		Vel = Context.Owner.EmitterToSimulation.TransformVector(Vel);
	}

	Vel *= OwnerScale;
	// Context.GetDistributionData() 는 현재 GetValue 내에서 안 쓰고 있음
	Vel += FromOrigin * StartVelocityRadial.GetValue(Context.Owner.EmitterTime) * OwnerScale;
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

	int32 Version = 1;
	Ar << Version;

	if (Version < 1)
	{
		FVector MinVelocity, MaxVelocity;
		Ar << MinVelocity;
		Ar << MaxVelocity;

		if (Ar.IsLoading())
		{
			StartVelocity.Distribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(this);
			if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(StartVelocity.Distribution))
			{
				Uniform->Min = MinVelocity;
				Uniform->Max = MaxVelocity;
			}
		}
	}
	else
	{
		StartVelocity.Serialize(Ar);
		StartVelocityRadial.Serialize(Ar);
	}

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
