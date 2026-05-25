#include "Particles/Beam/ParticleModuleBeamNoise.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Serialization/Archive.h"

#include <algorithm>

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
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData || !bLowFreq_Enabled)
	{
		return;
	}

	int32 CurrentOffset = Context.Owner.TypeDataOffset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FVector* InterpolatedPoints = nullptr;
	float* NoiseRate = nullptr;
	float* NoiseDeltaTime = nullptr;
	FVector* TargetNoisePoints = nullptr;
	FVector* NextNoisePoints = nullptr;
	float* TaperValues = nullptr;
	float* NoiseDistanceScale = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	BeamInst->BeamTypeData->GetDataPointers(&Context.Owner, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset,
		BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	if (!BeamData)
	{
		return;
	}

	const int32 NoiseCount = std::max(0, Frequency) + 1;
	BEAM2_TYPEDATA_SETFREQUENCY(BeamData->Lock_Max_NumNoisePoints, std::max(0, Frequency));
	if (NoiseRate)
	{
		*NoiseRate = 0.0f;
	}
	if (NoiseDeltaTime)
	{
		*NoiseDeltaTime = 0.0f;
	}
	if (TargetNoisePoints)
	{
		for (int32 NoiseIndex = 0; NoiseIndex < NoiseCount; ++NoiseIndex)
		{
			const float Alpha = NoiseCount > 1 ? static_cast<float>(NoiseIndex) / static_cast<float>(NoiseCount - 1) : 0.0f;
			const FVector Range = NoiseRange.GetValue(Context.Owner.EmitterTime + Alpha, Context.GetDistributionData());
			TargetNoisePoints[NoiseIndex] = Range;
			if (NextNoisePoints)
			{
				NextNoisePoints[NoiseIndex] = Range;
			}
		}
	}
	if (NoiseDistanceScale)
	{
		*NoiseDistanceScale = 1.0f;
	}
}

void UParticleModuleBeamNoise::Update(const FUpdateContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData || !bLowFreq_Enabled)
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	int32 CurrentOffset = Context.Owner.TypeDataOffset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FVector* InterpolatedPoints = nullptr;
	float* NoiseRate = nullptr;
	float* NoiseDeltaTime = nullptr;
	FVector* TargetNoisePoints = nullptr;
	FVector* NextNoisePoints = nullptr;
	float* TaperValues = nullptr;
	float* NoiseDistanceScale = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	BeamInst->BeamTypeData->GetDataPointers(&Context.Owner, ParticleBase, CurrentOffset,
		BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	if (NoiseDeltaTime)
	{
		*NoiseDeltaTime += Context.DeltaTime;
	}
	if (NoiseRate)
	{
		*NoiseRate = NoiseLockTime > 0.0f ? std::min(1.0f, *NoiseDeltaTime / NoiseLockTime) : 1.0f;
	}
	END_UPDATE_LOOP;
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
