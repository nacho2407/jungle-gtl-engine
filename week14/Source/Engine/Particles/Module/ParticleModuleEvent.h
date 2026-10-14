#pragma once

#include "Particles/Module/ParticleModule.h"
#include "Particles/Runtime/ParticleCollisionEvent.h"

#include "Source/Engine/Particles/Module/ParticleModuleEvent.generated.h"

UENUM()
enum class EParticleReceiveAction : uint8
{
	None = 0,
	Spawn = 1,
	Kill = 2
};

UCLASS()
class UParticleModuleEventGenerator : public UParticleModule
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Spawn")
	bool bGenerateSpawnEvents = false;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Spawn Event Name")
	FName SpawnEventName = FName("Spawn");

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Kill")
	bool bGenerateKillEvents = false;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Kill Event Name")
	FName KillEventName = FName("Kill");

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Collision")
	bool bGenerateCollisionEvents = true;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Collision Event Name")
	FName CollisionEventName = FName("Collision");
};

UCLASS()
class UParticleModuleEventReceiver : public UParticleModule
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Event Type", Enum=EParticleEventType)
	EParticleEventType EventType = EParticleEventType::Collision;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Event Name")
	FName EventName = FName("Collision");

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Action", Enum=EParticleReceiveAction)
	EParticleReceiveAction Action = EParticleReceiveAction::Spawn;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Spawn Count", Min=0.0f, Speed=1.0f)
	int32 SpawnCount = 1;

	UPROPERTY(Edit, Save, Category="Particle|Event", DisplayName="Inherit Velocity")
	bool bInheritVelocity = false;

	bool Matches(const FParticleCollisionEventPayload& Event) const;
	void ReceiveEvent(FParticleEmitterInstance* Owner, const FParticleCollisionEventPayload& Event) const;
};
