#include "Particles/Beam/ParticleModuleBeamNoise.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"

#include <algorithm>

namespace
{
	bool IsNoiseRangeUniform(const FRawDistributionVector& Distribution)
	{
		return dynamic_cast<UDistributionVectorUniform*>(Distribution.Distribution) != nullptr;
	}

	FVector GetNoiseRangeValue(const FRawDistributionVector& Distribution, float Time, UObject* Data, int32 Extreme, FRandomStream* RandomStream)
	{
		if (Extreme != 0)
		{
			FVector MinValue = FVector::ZeroVector;
			FVector MaxValue = FVector::ZeroVector;
			if (Distribution.Distribution)
			{
				Distribution.Distribution->GetRange(MinValue, MaxValue);
			}
			else
			{
				float Values[3] = { 0.0f, 0.0f, 0.0f };
				static_cast<const FRawDistribution&>(Distribution).GetValue(Time, Values, 3, Extreme, RandomStream);
				return FVector(Values[0], Values[1], Values[2]);
			}
			return Extreme > 0 ? MaxValue : MinValue;
		}

		return Distribution.GetValue(Time, Data, RandomStream);
	}
}

UParticleModuleBeamNoise::UParticleModuleBeamNoise()
	: bLowFreq_Enabled(false)
	, NoiseLockRadius(1.0f)
	, bNRScaleEmitterTime(false)
	, bSmooth(false)
	, bNoiseLock(false)
	, bOscillate(false)
	, NoiseLockTime(0.0f)
	, NoiseTension(0.5f)
	, bUseNoiseTangents(false)
	, NoiseTessellation(1)
	, bTargetNoise(false)
	, bApplyNoiseScale(false)
{
	InitializeDefaults();
}

void UParticleModuleBeamNoise::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;

	if (!NoiseSpeed.IsCreated())
	{
		UDistributionVectorConstant* DistributionNoiseSpeed = new UDistributionVectorConstant();
		DistributionNoiseSpeed->Constant = FVector(50.0f, 50.0f, 50.0f);
		NoiseSpeed.Distribution = DistributionNoiseSpeed;
	}

	if (!NoiseRange.IsCreated())
	{
		UDistributionVectorConstant* DistributionNoiseRange = new UDistributionVectorConstant();
		DistributionNoiseRange->Constant = FVector(50.0f, 50.0f, 50.0f);
		NoiseRange.Distribution = DistributionNoiseRange;
	}

	if (!NoiseRangeScale.IsCreated())
	{
		UDistributionFloatConstant* DistributionNoiseRangeScale = new UDistributionFloatConstant();
		DistributionNoiseRangeScale->Constant = 1.0f;
		NoiseRangeScale.Distribution = DistributionNoiseRangeScale;
	}

	if (!NoiseTangentStrength.IsCreated())
	{
		UDistributionFloatConstant* DistributionNoiseTangentStrength = new UDistributionFloatConstant();
		DistributionNoiseTangentStrength->Constant = 250.0f;
		NoiseTangentStrength.Distribution = DistributionNoiseTangentStrength;
	}

	if (!NoiseScale.IsCreated())
	{
		UDistributionFloatConstant* DistributionNoiseScale = new UDistributionFloatConstant();
		DistributionNoiseScale->Constant = 1.0f;
		NoiseScale.Distribution = DistributionNoiseScale;
	}
}

void UParticleModuleBeamNoise::Spawn(const FSpawnContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData || !bLowFreq_Enabled || Frequency == 0 || !Context.Owner.bIsBeam)
	{
		return;
	}

	FRandomStream RandomStream;

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
	if (!TargetNoisePoints)
	{
		return;
	}
	if (bSmooth && !NextNoisePoints)
	{
		return;
	}

	int32 CalcFreq = Frequency;
	if (Frequency_LowRange > 0)
	{
		const int32 Range = std::max(0, Frequency - Frequency_LowRange);
		CalcFreq = static_cast<int32>((RandomStream.FRand() * static_cast<float>(Range)) + static_cast<float>(Frequency_LowRange));
	}

	BEAM2_TYPEDATA_SETFREQUENCY(BeamData->Lock_Max_NumNoisePoints, std::max(0, CalcFreq));
	if (NoiseRate)
	{
		*NoiseRate = 0.0f;
	}
	if (NoiseDeltaTime)
	{
		*NoiseDeltaTime = 0.0f;
	}

	const float StepSize = 1.0f / static_cast<float>(CalcFreq + 1);
	const bool bLocalOscillate = IsNoiseRangeUniform(NoiseRange);
	int32 Extreme = -1;
	for (int32 NoiseIndex = 0; NoiseIndex < (CalcFreq + 1); ++NoiseIndex)
	{
		if (bLocalOscillate && bOscillate)
		{
			Extreme = -Extreme;
		}
		else
		{
			Extreme = 0;
		}

		TargetNoisePoints[NoiseIndex] = GetNoiseRangeValue(NoiseRange, StepSize * static_cast<float>(NoiseIndex), Context.GetDistributionData(), Extreme, &RandomStream);
		if (bSmooth)
		{
			Extreme = -Extreme;
			NextNoisePoints[NoiseIndex] = GetNoiseRangeValue(NoiseRange, StepSize * static_cast<float>(NoiseIndex), Context.GetDistributionData(), Extreme, &RandomStream);
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
	if (Frequency == 0 || !Context.Owner.bIsBeam)
	{
		return;
	}

	const bool bLocalOscillate = IsNoiseRangeUniform(NoiseRange);
	int32 Extreme = -1;

	BEGIN_UPDATE_LOOP;
	{
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

	if (!BeamData || !TargetNoisePoints)
	{
		continue;
	}
	if (bSmooth && !NextNoisePoints)
	{
		continue;
	}

	const int32 Freq = BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints);
	if (bLocalOscillate && bOscillate)
	{
		Extreme = -Extreme;
	}
	else
	{
		Extreme = 0;
	}

	if (NoiseLockTime < 0.0f)
	{
		// UE original responsibility: leave noise points locked forever when NoiseLockTime is negative.
	}
	else
	{
		const float StepSize = 1.0f / static_cast<float>(Freq + 1);
		FRandomStream RandomStream;
		if (NoiseLockTime > 1.0e-4f)
		{
			if (!NoiseRate || !NoiseDeltaTime)
			{
				continue;
			}

			*NoiseRate += Context.DeltaTime;
			if (*NoiseRate > NoiseLockTime)
			{
				if (bSmooth)
				{
					for (int32 NoiseIndex = 0; NoiseIndex < (Freq + 1); ++NoiseIndex)
					{
						NextNoisePoints[NoiseIndex] = GetNoiseRangeValue(NoiseRange, StepSize * static_cast<float>(NoiseIndex), Context.GetDistributionData(), Extreme, &RandomStream);
					}
				}
				else
				{
					for (int32 NoiseIndex = 0; NoiseIndex < (Freq + 1); ++NoiseIndex)
					{
						TargetNoisePoints[NoiseIndex] = GetNoiseRangeValue(NoiseRange, StepSize * static_cast<float>(NoiseIndex), Context.GetDistributionData(), Extreme, &RandomStream);
					}
				}
				*NoiseRate = 0.0f;
			}
			*NoiseDeltaTime = Context.DeltaTime;
		}
		else
		{
			for (int32 NoiseIndex = 0; NoiseIndex < (Freq + 1); ++NoiseIndex)
			{
				TargetNoisePoints[NoiseIndex] = GetNoiseRangeValue(NoiseRange, StepSize * static_cast<float>(NoiseIndex), Context.GetDistributionData(), Extreme, &RandomStream);
			}
		}
	}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleBeamNoise::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
	if (NoiseRange.Distribution)
	{
		NoiseRange.Distribution->GetRange(NoiseMin, NoiseMax);
	}
	else
	{
		float Min = 0.0f;
		float Max = 0.0f;
		static_cast<const FRawDistribution&>(NoiseRange).GetValue(0.0f, &Min, 1, -1, nullptr);
		static_cast<const FRawDistribution&>(NoiseRange).GetValue(0.0f, &Max, 1, 1, nullptr);
		NoiseMin = FVector(Min, Min, Min);
		NoiseMax = FVector(Max, Max, Max);
	}
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
