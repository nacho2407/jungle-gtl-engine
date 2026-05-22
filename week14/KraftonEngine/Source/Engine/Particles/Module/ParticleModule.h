#pragma once

#include "Object/Object.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"

struct FParticleEmitterInstance;

class UParticleModule : public UObject
{
public:
	virtual ~UParticleModule() = default;

	virtual bool IsSpawnModule() const { return false; }
	virtual bool IsUpdateModule() const { return false; }

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) {}
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime, FBaseParticle& Particle) {}
};

class UParticleModuleRequired : public UParticleModule
{
public:
	float EmitterDuration = 1.0f;
	bool bLooping = true;
};

class UParticleModuleSpawn : public UParticleModule
{
	public:
	float SpawnRate = 10.0f;

	virtual bool IsSpawnModule() const override { return true; }
};

class UParticleModuleLifeTime : public UParticleModule
{
public:
	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Lifetime = Lifetime;
		Particle.OneOverMaxLifetime = Lifetime > 0.0f ? 1.0f / Lifetime : 0.0f;
		Particle.Age = 0.0f;
		Particle.RelativeTime = 0.0f;
	}
private:
	float Lifetime = 1.0f;
};

class UParticleModuleLocation : public UParticleModule
{
public:
	FVector StartLocation = FVector::ZeroVector;

	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Position += StartLocation;
		Particle.OldPosition = Particle.Position;
	}

};

class UParticleModuleVelocity : public UParticleModule
{
public:
	bool IsSpawnModule() const override { return true; }

	FVector StartVelocity = FVector(0.0f, 0.0f, 100.0f);
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Velocity += StartVelocity;
	}
};

class UParticleModuleColor : public UParticleModule
{
public:
	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	FVector4 StartColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
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

class UPartticleModuleSize : public UParticleModule
{
public:
	bool IsSpawnModule() const override { return true; }
	FVector StartSize = FVector(10.0f, 10.0f, 1.0f);

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Size = StartSize;
	}
};