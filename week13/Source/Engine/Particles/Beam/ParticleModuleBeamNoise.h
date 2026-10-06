#pragma once

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particles/Beam/ParticleModuleBeamBase.h"

#include "Source/Engine/Particles/Beam/ParticleModuleBeamNoise.generated.h"

UCLASS()
class UParticleModuleBeamNoise : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	static const uint32 MaxNoiseTessellation = 500;

	uint32 bLowFreq_Enabled : 1;
	int32 Frequency = 0;
	int32 Frequency_LowRange = 0;
	FRawDistributionVector NoiseRange;
	FRawDistributionFloat NoiseRangeScale;
	uint32 bNRScaleEmitterTime : 1;
	FRawDistributionVector NoiseSpeed;
	uint32 bSmooth : 1;
	float NoiseLockRadius = 0.0f;
	uint32 bNoiseLock : 1;
	uint32 bOscillate : 1;
	float NoiseLockTime = 0.0f;
	float NoiseTension = 0.0f;
	uint32 bUseNoiseTangents : 1;
	FRawDistributionFloat NoiseTangentStrength;
	int32 NoiseTessellation = 0;
	uint32 bTargetNoise : 1;
	float FrequencyDistance = 0.0f;
	uint32 bApplyNoiseScale : 1;
	FRawDistributionFloat NoiseScale;

	UParticleModuleBeamNoise();
	void InitializeDefaults();
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	void GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax);
};
