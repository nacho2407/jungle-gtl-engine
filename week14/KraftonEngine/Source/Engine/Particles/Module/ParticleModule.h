#pragma once

#include "Object/Object.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Core/Property/SoftObjectProperty.h"

#include "Source/Engine/Particles/Module/ParticleModule.generated.h"

struct FParticleEmitterInstance;
class UMaterialInterface;

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY()
	virtual ~UParticleModule() = default;

	virtual bool IsSpawnModule() const { return false; }
	virtual bool IsUpdateModule() const { return false; }

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) {}
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime, FBaseParticle& Particle) {}
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY()
	
	UMaterialInterface* Material = nullptr;

	UPROPERTY(Edit, Save, Category = "Particle|Required", DisplayName = "Material", AssetType = "Material")
	FSoftObjectPtr MaterialPath = "None";

	UPROPERTY(Edit, Save, Category="Particle|Required", DisplayName="Emitter Duration", Min=0.0f, Speed=0.1f)
	float EmitterDuration = 1.0f;
	UPROPERTY(Edit, Save, Category="Particle|Required", DisplayName="Looping")
	bool bLooping = true;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY()
	UPROPERTY(Edit, Save, Category="Particle|Spawn", DisplayName="Spawn Rate", Min=0.0f, Speed=0.1f)
	float SpawnRate = 10.0f;

	virtual bool IsSpawnModule() const override { return true; }
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Lifetime = Lifetime;
		Particle.OneOverMaxLifetime = Lifetime > 0.0f ? 1.0f / Lifetime : 0.0f;
		Particle.Age = 0.0f;
		Particle.RelativeTime = 0.0f;
	}

	UPROPERTY(Edit, Save, Category="Particle|Lifetime", DisplayName="Lifetime", Min=0.0f, Speed=0.1f)
	float Lifetime = 1.0f;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY()
	UPROPERTY(Edit, Save, Category="Particle|Location", DisplayName="Start Location")
	FVector StartLocation = FVector::ZeroVector;

	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Position += StartLocation;
		Particle.OldPosition = Particle.Position;
	}

};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsSpawnModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Velocity", DisplayName="Start Velocity")
	FVector StartVelocity = FVector(0.0f, 100.0f, 0.0f);
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Velocity = StartVelocity;
	}
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="Start Color")
	FVector4 StartColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="End Color")
	FVector4 EndColor = FVector4(1.0f, 1.0f, 1.0f, 0.0f);

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Color = StartColor;
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime, FBaseParticle& Particle) override
	{
		const float T = Particle.RelativeTime;
		Particle.Color = StartColor * (1.0f - T) + EndColor * T;
	}
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsSpawnModule() const override { return true; }
	UPROPERTY(Edit, Save, Category="Particle|Size", DisplayName="Start Size", Min=0.0f, Speed=0.1f)
	FVector StartSize = FVector(10.0f, 10.0f, 1.0f);

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Size = StartSize;
	}
};
