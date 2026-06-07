#pragma once

#include "Object/Object.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Particles/ParticleHelper.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Math/Distribution.h"

#include "Source/Engine/Particles/Module/ParticleModule.generated.h"

class UMaterialInterface;

UENUM()
enum class EParticleSpawnType : uint8
{
	Default,
	Burst,
};

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

	UPROPERTY(Edit, Save, Category="Particle|Spawn", DisplayName="Spawn Type", Enum=EParticleSpawnType)
	EParticleSpawnType SpawnType = EParticleSpawnType::Default;

	UPROPERTY(Edit, Save, Category="Particle|Spawn|Burst", DisplayName="Burst Count", Min=0.0f, Speed=1.0f, EditCondition="SpawnType == Burst")
	int32 BurstCount = 1;

	UPROPERTY(Edit, Save, Category="Particle|Spawn|Burst", DisplayName="Burst Time", Min=0.0f, Speed=0.01f, EditCondition="SpawnType == Burst")
	float BurstTime = 0.0f;

	UPROPERTY(Edit, Save, Category="Particle|Spawn|Burst", DisplayName="Repeat Burst On Loop", EditCondition="SpawnType == Burst")
	bool bRepeatBurstOnLoop = true;

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
		const float ScaleMultiplier = Owner ? Owner->GetParticleScaleMultiplier() : 1.0f;
		Particle.Position += StartLocation.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartLocation")) * ScaleMultiplier;
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
		const float ScaleMultiplier = Owner ? Owner->GetParticleScaleMultiplier() : 1.0f;
		Particle.Velocity = StartVelocity.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartVelocity")) * ScaleMultiplier;
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
			const float ScaleMultiplier = Context.Owner.GetParticleScaleMultiplier();
			const FVector FrameAcceleration = Acceleration.GetValue(Particle->RelativeTime, RandomFraction) * ScaleMultiplier;
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
		(void)Owner;
		(void)Offset;
		(void)SpawnTime;

		// Color Over Life는 emitter time이 아니라 particle relative time을 기준으로 평가되어야 한다.
		// Spawn 시에는 각 particle의 시작/끝 색을 고정해 두고, Update에서 RelativeTime으로 보간한다.
		Particle.InitialColor = EvaluateStartColor(Particle, 0.0f);
		Particle.TargetColor = EvaluateEndColor(Particle, 1.0f);
		Particle.Color = Particle.InitialColor;
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		(void)DeltaTime;
		struct
		{
			FParticleEmitterInstance& Owner;
			int32 Offset;
			float DeltaTime;
		} Context{ *Owner, Offset, DeltaTime };

		BEGIN_UPDATE_LOOP
			const float T = FMath::Clamp(Particle->RelativeTime, 0.0f, 1.0f);

			// Constant/Uniform 값은 Spawn에서 고정한 per-particle start/end 색을 보간한다.
			// Curve 모드는 기존 에셋 의미를 유지하기 위해 relative time으로 분포를 다시 평가한다.
			FVector4 Start = Particle->InitialColor;
			FVector4 End = Particle->TargetColor;
			if (UsesTimeVaryingDistribution())
			{
				Start = EvaluateStartColor(*Particle, T);
				End = EvaluateEndColor(*Particle, T);
			}

			Particle->Color = LerpColor(Start, End, T);
		END_UPDATE_LOOP
	}

private:
	static bool IsTimeVarying(EDistributionValueMode Mode)
	{
		return Mode == EDistributionValueMode::ConstantCurve || Mode == EDistributionValueMode::UniformCurve;
	}

	bool UsesTimeVaryingDistribution() const
	{
		return IsTimeVarying(StartColor.Mode)
			|| IsTimeVarying(StartAlpha.Mode)
			|| IsTimeVarying(EndColor.Mode)
			|| IsTimeVarying(EndAlpha.Mode);
	}

	FVector4 EvaluateStartColor(const FBaseParticle& Particle, float Time) const
	{
		const FVector RGB = StartColor.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "ColorOverLife.StartColor"));
		const float Alpha = StartAlpha.GetValue(Time, FDistributionSampling::RandomUnit(Particle.RandomSeed, "ColorOverLife.StartAlpha"));
		return FVector4(RGB.X, RGB.Y, RGB.Z, Alpha);
	}

	FVector4 EvaluateEndColor(const FBaseParticle& Particle, float Time) const
	{
		const FVector RGB = EndColor.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "ColorOverLife.EndColor"));
		const float Alpha = EndAlpha.GetValue(Time, FDistributionSampling::RandomUnit(Particle.RandomSeed, "ColorOverLife.EndAlpha"));
		return FVector4(RGB.X, RGB.Y, RGB.Z, Alpha);
	}

	static FVector4 LerpColor(const FVector4& A, const FVector4& B, float T)
	{
		return FVector4(
			FMath::Lerp(A.X, B.X, T),
			FMath::Lerp(A.Y, B.Y, T),
			FMath::Lerp(A.Z, B.Z, T),
			FMath::Lerp(A.W, B.W, T));
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
		const float ScaleMultiplier = Owner ? Owner->GetParticleScaleMultiplier() : 1.0f;
		Particle.Size = StartSize.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartSize")) * ScaleMultiplier;
		Particle.InitialSize = Particle.Size;
		Particle.TargetSize = Particle.Size;
	}
};

UCLASS()
class UParticleModuleSizeOverLife : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSizeOverLife()
	{
		StartSize.Constant = FVector(25.0f, 25.0f, 25.0f);
		StartSize.MinValue = StartSize.Constant;
		StartSize.MaxValue = StartSize.Constant;

		EndSize.Constant = FVector::ZeroVector;
		EndSize.MinValue = EndSize.Constant;
		EndSize.MaxValue = EndSize.Constant;
	}

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Size", DisplayName="Start Size", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartSize;
	UPROPERTY(Edit, Save, Category="Particle|Size", DisplayName="End Size", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector EndSize;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		(void)Owner;
		(void)Offset;
		(void)SpawnTime;

		// Color Over Life와 동일한 방식으로 Spawn 시 per-particle start/end 값을 고정하고,
		// Update에서 particle relative time 기준으로 보간한다.
		const float ScaleMultiplier = Owner ? Owner->GetParticleScaleMultiplier() : 1.0f;
		Particle.InitialSize = EvaluateStartSize(Particle, 0.0f) * ScaleMultiplier;
		Particle.TargetSize = EvaluateEndSize(Particle, 1.0f) * ScaleMultiplier;
		Particle.Size = Particle.InitialSize;
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		(void)DeltaTime;
		struct
		{
			FParticleEmitterInstance& Owner;
			int32 Offset;
			float DeltaTime;
		} Context{ *Owner, Offset, DeltaTime };

		BEGIN_UPDATE_LOOP
			const float T = FMath::Clamp(Particle->RelativeTime, 0.0f, 1.0f);

			// Constant/Uniform 값은 Spawn에서 고정한 per-particle start/end size를 보간한다.
			// Curve 모드는 Color Over Life와 같이 relative time으로 분포를 다시 평가한다.
			FVector Start = Particle->InitialSize;
			FVector End = Particle->TargetSize;
			if (UsesTimeVaryingDistribution())
			{
				const float ScaleMultiplier = Context.Owner.GetParticleScaleMultiplier();
				Start = EvaluateStartSize(*Particle, T) * ScaleMultiplier;
				End = EvaluateEndSize(*Particle, T) * ScaleMultiplier;
			}

			Particle->Size = LerpSize(Start, End, T);
		END_UPDATE_LOOP
	}

private:
	static bool IsTimeVarying(EDistributionValueMode Mode)
	{
		return Mode == EDistributionValueMode::ConstantCurve || Mode == EDistributionValueMode::UniformCurve;
	}

	bool UsesTimeVaryingDistribution() const
	{
		return IsTimeVarying(StartSize.Mode)
			|| IsTimeVarying(EndSize.Mode);
	}

	FVector EvaluateStartSize(const FBaseParticle& Particle, float Time) const
	{
		return StartSize.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "SizeOverLife.StartSize"));
	}

	FVector EvaluateEndSize(const FBaseParticle& Particle, float Time) const
	{
		return EndSize.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "SizeOverLife.EndSize"));
	}

	static FVector LerpSize(const FVector& A, const FVector& B, float T)
	{
		return FVector(
			FMath::Lerp(A.X, B.X, T),
			FMath::Lerp(A.Y, B.Y, T),
			FMath::Lerp(A.Z, B.Z, T));
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
