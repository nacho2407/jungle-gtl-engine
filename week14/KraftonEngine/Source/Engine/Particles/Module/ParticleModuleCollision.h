#pragma once

#include "Core/Types/CollisionTypes.h"
#include "Particles/Module/ParticleModule.h"

#include "Source/Engine/Particles/Module/ParticleModuleCollision.generated.h"

UENUM()
enum class EParticleCollisionResponse : uint8
{
	Bounce = 0,
	Stop = 1,
	Kill = 2
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY()

	bool IsUpdateModule() const override { return true; }
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Enabled")
	bool bEnabled = true;
	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Trace Channel", Enum=ECollisionChannel)
	ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic;
	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Response", Enum=EParticleCollisionResponse)
	EParticleCollisionResponse Response = EParticleCollisionResponse::Bounce;
	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Radius", Min=0.0f, Speed=0.1f)
	float Radius = 1.0f;
	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Restitution", Min=0.0f, Speed=0.05f)
	float Restitution = 0.5f;
	UPROPERTY(Edit, Save, Category="Particle|Collision", DisplayName="Friction", Min=0.0f, Max=1.0f, Speed=0.05f)
	float Friction = 0.1f;
};
