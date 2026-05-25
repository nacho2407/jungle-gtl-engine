#include "Particles/Beam/ParticleModuleBeamNoise.h"

#include "Serialization/Archive.h"

UParticleModuleBeamNoise::UParticleModuleBeamNoise()
	: bLowFreq_Enabled(false)
	, bNRScaleEmitterTime(false)
	, bSmooth(false)
	, bNoiseLock(false)
	, bOscillate(false)
	, bUseNoiseTangents(false)
	, bTargetNoise(false)
	, bApplyNoiseScale(false)
{
	InitializeDefaults();
}

void UParticleModuleBeamNoise::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
	Frequency = 0;
	NoiseTessellation = 1;
}

void UParticleModuleBeamNoise::Spawn(const FSpawnContext& Context)
{
	// UE original responsibility: seed low-frequency noise payload arrays.
	// Missing Jungle foundation: full beam noise payload arrays are allocated by TypeData,
	// but the UE noise evolution code has not been ported yet.
	// System to connect later: ParticleBeamModules.cpp BeamNoise::Spawn.
}

void UParticleModuleBeamNoise::Update(const FUpdateContext& Context)
{
	// UE original responsibility: advance beam noise points, rates, deltas and distance scale.
	// Missing Jungle foundation: Cascade noise interpolation payload evolution path.
	// System to connect later: ParticleBeamModules.cpp BeamNoise::Update.
}

void UParticleModuleBeamNoise::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
	const FVector Range = NoiseRange.GetValue(0.0f);
	NoiseMin = -Range;
	NoiseMax = Range;
}

void UParticleModuleBeamNoise::Serialize(FArchive& Ar)
{
	UParticleModuleBeamBase::Serialize(Ar);
	bool LowFreqEnabled = bLowFreq_Enabled;
	Ar << LowFreqEnabled << Frequency << Frequency_LowRange;
	NoiseRange.Serialize(Ar);
	NoiseRangeScale.Serialize(Ar);
	bool NRScaleEmitterTime = bNRScaleEmitterTime;
	Ar << NRScaleEmitterTime;
	NoiseSpeed.Serialize(Ar);
	bool Smooth = bSmooth;
	bool NoiseLock = bNoiseLock;
	bool Oscillate = bOscillate;
	bool UseNoiseTangents = bUseNoiseTangents;
	Ar << Smooth << NoiseLockRadius << NoiseLock << Oscillate << NoiseLockTime << NoiseTension << UseNoiseTangents;
	NoiseTangentStrength.Serialize(Ar);
	bool TargetNoise = bTargetNoise;
	bool ApplyNoiseScale = bApplyNoiseScale;
	Ar << NoiseTessellation << TargetNoise << FrequencyDistance << ApplyNoiseScale;
	NoiseScale.Serialize(Ar);
	if (Ar.IsLoading())
	{
		bLowFreq_Enabled = LowFreqEnabled;
		bNRScaleEmitterTime = NRScaleEmitterTime;
		bSmooth = Smooth;
		bNoiseLock = NoiseLock;
		bOscillate = Oscillate;
		bUseNoiseTangents = UseNoiseTangents;
		bTargetNoise = TargetNoise;
		bApplyNoiseScale = ApplyNoiseScale;
	}
}
