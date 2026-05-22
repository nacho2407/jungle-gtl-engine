#include "Particles/ParticleRuntimeTypes.h"

#include "Particles/ParticleSystem.h"

void FParticleEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	SpriteTemplate = InTemplate;
	Component = InComponent;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(CurrentLODLevelIndex) : nullptr;
	MaxActiveParticles = SpriteTemplate ? SpriteTemplate->GetMaxActiveParticles() : 0;
	Reset();
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!bActive || !SpriteTemplate)
	{
		return;
	}

	EmitterTime += DeltaTime;

	const float Duration = SpriteTemplate->GetEmitterDuration();
	if (Duration > 0.0f && EmitterTime >= Duration)
	{
		if (SpriteTemplate->IsLooping())
		{
			while (EmitterTime >= Duration)
			{
				EmitterTime -= Duration;
			}
		}
		else
		{
			EmitterTime = Duration;
			bActive = false;
		}
	}
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles = 0;
	ParticleCounter = 0;
	EmitterTime = 0.0f;
	bActive = true;
}
