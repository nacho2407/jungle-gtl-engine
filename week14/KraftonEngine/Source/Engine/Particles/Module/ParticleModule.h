#pragma once

#include "Object/Object.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Particles/ParticleHelper.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Math/Distribution.h"

#include "Source/Engine/Particles/Module/ParticleModule.generated.h"

class UMaterialInterface;

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY()
	virtual ~UParticleModule() = default;

	virtual bool IsSpawnModule() const { return false; }
	virtual bool IsUpdateModule() const { return false; }
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) {}
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

private:
	UPROPERTY(Save, Category="Particle|Module", DisplayName="Enabled")
	bool bEnabled = true;
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
	UParticleModuleLifetime()
	{
		Lifetime.Constant = 1.0f;
		Lifetime.MinValue = 1.0f;
		Lifetime.MaxValue = 1.0f;
	}

	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Lifetime = Lifetime.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "Lifetime"));
		Particle.OneOverMaxLifetime = Particle.Lifetime > 0.0f ? 1.0f / Particle.Lifetime : 0.0f;
		Particle.Age = 0.0f;
		Particle.RelativeTime = 0.0f;
	}

	UPROPERTY(Edit, Save, Category="Particle|Lifetime", DisplayName="Lifetime", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat Lifetime;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY()
	UPROPERTY(Edit, Save, Category="Particle|Location", DisplayName="Start Location", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartLocation;

	bool IsSpawnModule() const override { return true; }

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Position += StartLocation.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartLocation"));
		Particle.OldPosition = Particle.Position;
	}

};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleVelocity()
	{
		StartVelocity.Constant = FVector(0.0f, 100.0f, 0.0f);
		StartVelocity.MinValue = StartVelocity.Constant;
		StartVelocity.MaxValue = StartVelocity.Constant;
	}

	bool IsSpawnModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Velocity", DisplayName="Start Velocity", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartVelocity;
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Velocity = StartVelocity.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartVelocity"));
	}
};

UCLASS()
class UParticleModuleAcceleration : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleAcceleration()
	{
		Acceleration.Constant = FVector::ZeroVector;
		Acceleration.MinValue = Acceleration.Constant;
		Acceleration.MaxValue = Acceleration.Constant;
	}

	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Acceleration", DisplayName="Acceleration", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector Acceleration;

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		struct
		{
			FParticleEmitterInstance& Owner;
			int32 Offset;
			float DeltaTime;
		} Context{ *Owner, Offset, DeltaTime };

		BEGIN_UPDATE_LOOP
			const FVector RandomFraction = FDistributionSampling::RandomUnitVector(Particle->RandomSeed, "Acceleration");
			const FVector FrameAcceleration = Acceleration.GetValue(Particle->RelativeTime, RandomFraction);
			Particle->Velocity += FrameAcceleration * DeltaTime;
		END_UPDATE_LOOP
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

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		struct
		{
			FParticleEmitterInstance& Owner;
			int32 Offset;
			float DeltaTime;
		} Context{ *Owner, Offset, DeltaTime };

		BEGIN_UPDATE_LOOP
			const float T = Particle->RelativeTime;
			Particle->Color = StartColor * (1.0f - T) + EndColor * T;
		END_UPDATE_LOOP
	}
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSize()
	{
		StartSize.Constant = FVector(10.0f, 10.0f, 1.0f);
		StartSize.MinValue = StartSize.Constant;
		StartSize.MaxValue = StartSize.Constant;
	}

	bool IsSpawnModule() const override { return true; }
	UPROPERTY(Edit, Save, Category="Particle|Size", DisplayName="Start Size", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartSize;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.Size = StartSize.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartSize"));
	}
};
