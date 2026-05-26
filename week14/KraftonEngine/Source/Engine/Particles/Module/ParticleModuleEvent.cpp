#include "ParticleModuleEvent.h"

bool UParticleModuleEventReceiver::Matches(const FParticleCollisionEventPayload& Event) const
{
	if (Event.EventType != EventType)
	{
		return false;
	}

	return !EventName.IsValid() || EventName == FName::None || EventName == Event.EventName;
}

void UParticleModuleEventReceiver::ReceiveEvent(FParticleEmitterInstance* Owner, const FParticleCollisionEventPayload& Event) const
{
	if (!Owner || !Matches(Event))
	{
		return;
	}

	if (Action == EParticleReceiveAction::None)
	{
		return;
	}

	if (Action == EParticleReceiveAction::Kill)
	{
		while (Owner->ActiveParticles > 0)
		{
			Owner->KillParticle(0);
		}
		return;
	}

	if (Action != EParticleReceiveAction::Spawn || SpawnCount <= 0)
	{
		return;
	}

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		FBaseParticle* Particle = Owner->SpawnParticle();
		if (!Particle)
		{
			return;
		}

		Owner->InitializeParticle(*Particle, Event.Location);
		if (bInheritVelocity)
		{
			Particle->Velocity = Event.Velocity;
		}
		++Owner->ParticleCounter;
	}
}
