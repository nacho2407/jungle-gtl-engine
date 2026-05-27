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

	UPROPERTY(Edit, Save, Category="Particle|Required|SubUV", DisplayName="Sub Images Horizontal", Min=1.0f, Speed=1.0f)
	int32 SubImagesHorizontal = 1;

	UPROPERTY(Edit, Save, Category="Particle|Required|SubUV", DisplayName="Sub Images Vertical", Min=1.0f, Speed=1.0f)
	int32 SubImagesVertical = 1;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSpawn()
	{
		SpawnRate.Constant = 20.0f;
		SpawnRate.MinValue = 20.0f;
		SpawnRate.MaxValue = 20.0f;
	}

	UPROPERTY(Edit, Save, Category="Particle|Spawn", DisplayName="Spawn Rate", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat SpawnRate;

	virtual bool IsSpawnModule() const override { return true; }
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLifetime()
	{
		Lifetime.Mode = EDistributionValueMode::Uniform;
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
		StartVelocity.Mode = EDistributionValueMode::Uniform;
		StartVelocity.Constant = FVector::ZeroVector;
		StartVelocity.MinValue = FVector(-10.0f, -10.0f, 50.0f);
		StartVelocity.MaxValue = FVector(10.0f, 10.0f, 100.0f);
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
class UParticleModuleRotation : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleRotation()
	{
		StartRotation.Constant = 0.0f;
		StartRotation.MinValue = 0.0f;
		StartRotation.MaxValue = 0.0f;
	}

	bool IsSpawnModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Rotation", DisplayName="Start Rotation", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat StartRotation;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		(void)Owner;
		(void)Offset;
		Particle.Rotation = StartRotation.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "StartRotation"));
	}
};

UCLASS()
class UParticleModuleRotationRate : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleRotationRate()
	{
		StartRotationRate.Constant = 0.0f;
		StartRotationRate.MinValue = 0.0f;
		StartRotationRate.MaxValue = 0.0f;
	}

	bool IsSpawnModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Rotation", DisplayName="Start Rotation Rate", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat StartRotationRate;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		(void)Owner;
		(void)Offset;
		Particle.RotationRate = StartRotationRate.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "StartRotationRate"));
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
	UParticleModuleColor()
	{
		StartColor.Constant = FVector(1.0f, 1.0f, 1.0f);
		StartColor.MinValue = StartColor.Constant;
		StartColor.MaxValue = StartColor.Constant;

		StartAlpha.Constant = 1.0f;
		StartAlpha.MinValue = 1.0f;
		StartAlpha.MaxValue = 1.0f;

		EndColor.Constant = FVector(1.0f, 1.0f, 1.0f);
		EndColor.MinValue = EndColor.Constant;
		EndColor.MaxValue = EndColor.Constant;

		EndAlpha.Constant = 0.0f;
		EndAlpha.MinValue = 0.0f;
		EndAlpha.MaxValue = 0.0f;
	}

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="Start Color", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartColor;
	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="Start Alpha", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat StartAlpha;
	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="End Color", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector EndColor;
	UPROPERTY(Edit, Save, Category="Particle|Color", DisplayName="End Alpha", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat EndAlpha;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		const FVector RGB = StartColor.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartColor"));
		const float Alpha = StartAlpha.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "StartAlpha"));
		Particle.Color = FVector4(RGB.X, RGB.Y, RGB.Z, Alpha);
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
			const FVector StartRGB = StartColor.GetValue(T, FDistributionSampling::RandomUnitVector(Particle->RandomSeed, "StartColor"));
			const FVector EndRGB = EndColor.GetValue(T, FDistributionSampling::RandomUnitVector(Particle->RandomSeed, "EndColor"));
			const float StartA = StartAlpha.GetValue(T, FDistributionSampling::RandomUnit(Particle->RandomSeed, "StartAlpha"));
			const float EndA = EndAlpha.GetValue(T, FDistributionSampling::RandomUnit(Particle->RandomSeed, "EndAlpha"));
			const FVector RGB = StartRGB * (1.0f - T) + EndRGB * T;
			const float Alpha = StartA * (1.0f - T) + EndA * T;
			Particle->Color = FVector4(RGB.X, RGB.Y, RGB.Z, Alpha);
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
		StartSize.Mode = EDistributionValueMode::Uniform;
		StartSize.Constant = FVector(25.0f, 25.0f, 25.0f);
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

UCLASS()
class UParticleModuleSubImageIndex : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSubImageIndex()
	{
		SubImageIndex.Constant = 0.0f;
		SubImageIndex.MinValue = 0.0f;
		SubImageIndex.MaxValue = 0.0f;
	}

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|SubUV", DisplayName="Sub Image Index", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat SubImageIndex;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		Particle.SubImageIndex = SubImageIndex.GetValue(0.0f, FDistributionSampling::RandomUnit(Particle.RandomSeed, "SubImageIndex"));
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
			const float RandomFraction = FDistributionSampling::RandomUnit(Particle->RandomSeed, "SubImageIndex");
			Particle->SubImageIndex = SubImageIndex.GetValue(Particle->RelativeTime, RandomFraction);
		END_UPDATE_LOOP
	}
};
