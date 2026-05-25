#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"

#include "Particles/Beam/ParticleModuleBeamModifier.h"
#include "Particles/Beam/ParticleModuleBeamNoise.h"
#include "Particles/Beam/ParticleModuleBeamSource.h"
#include "Particles/Beam/ParticleModuleBeamTarget.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

namespace
{
	FVector GetBeamEmitterXAxis(const FParticleEmitterInstance& Owner)
	{
		if (Owner.Component)
		{
			return Owner.Component->GetWorldMatrix().TransformVector(FVector::XAxisVector).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		}
		return FVector::XAxisVector;
	}

	FVector GetBeamEmitterLocation(const FParticleEmitterInstance& Owner)
	{
		return Owner.Component ? Owner.Component->GetWorldLocation() : Owner.Location;
	}

	FVector CubicInterpVector(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float Alpha)
	{
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;
		return P0 * (2.0f * A3 - 3.0f * A2 + 1.0f)
			+ T0 * (A3 - 2.0f * A2 + Alpha)
			+ P1 * (-2.0f * A3 + 3.0f * A2)
			+ T1 * (A3 - A2);
	}
}

UParticleModuleTypeDataBeam2::UParticleModuleTypeDataBeam2()
	: bAlwaysOn(false)
	, RenderGeometry(true)
	, RenderDirectLine(false)
	, RenderLines(false)
	, RenderTessellation(false)
{
	InitializeDefaults();
}

void UParticleModuleTypeDataBeam2::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
	MaxBeamCount = std::max(1, MaxBeamCount);
	Sheets = std::max(1, Sheets);
}

UParticleModuleBeamNoise* GetFirstBeamNoiseModuleForLayout(UParticleModuleTypeDataBeam2* TypeData)
{
	return (TypeData && TypeData->LOD_BeamModule_Noise.size() > 0) ? TypeData->LOD_BeamModule_Noise[0] : nullptr;
}

uint32 UParticleModuleTypeDataBeam2::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	uint32 Size = 0;
	int32 TaperCount = 2;

	Size += sizeof(FBeam2TypeDataPayload);

	if (InterpolationPoints > 0)
	{
		Size += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	UParticleModuleBeamNoise* BeamNoise = GetFirstBeamNoiseModuleForLayout(this);
	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		Size += sizeof(float);              // NoiseRate
		Size += sizeof(float);              // NoiseDeltaTime
		Size += sizeof(FVector) * Frequency; // TargetNoisePoints

		if (BeamNoise->bSmooth)
		{
			Size += sizeof(FVector) * Frequency; // NextNoisePoints
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			Size += sizeof(float); // NoiseDistanceScale
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		Size += sizeof(float) * TaperCount;
	}

	return Size;
}

void UParticleModuleTypeDataBeam2::GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, FBeam2TypeDataPayload*& BeamData, FVector*& InterpolatedPoints,
	float*& NoiseRate, float*& NoiseDeltaTime, FVector*& TargetNoisePoints,
	FVector*& NextNoisePoints, float*& TaperValues, float*& NoiseDistanceScale,
	FBeamParticleModifierPayloadData*& SourceModifier,
	FBeamParticleModifierPayloadData*& TargetModifier)
{
	uint8* Base = const_cast<uint8*>(ParticleBase);
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	UParticleModuleBeamNoise* BeamNoise = BeamInst ? BeamInst->BeamModule_Noise : nullptr;
	int32 TaperCount = 2;

	BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FBeam2TypeDataPayload);

	InterpolatedPoints = nullptr;
	NoiseRate = nullptr;
	NoiseDeltaTime = nullptr;
	TargetNoisePoints = nullptr;
	NextNoisePoints = nullptr;
	TaperValues = nullptr;
	NoiseDistanceScale = nullptr;

	if (InterpolationPoints > 0)
	{
		InterpolatedPoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
		CurrentOffset += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		NoiseRate = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float);
		NoiseDeltaTime = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float);

		TargetNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
		CurrentOffset += sizeof(FVector) * Frequency;

		if (BeamNoise->bSmooth)
		{
			NextNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
			CurrentOffset += sizeof(FVector) * Frequency;
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			NoiseDistanceScale = reinterpret_cast<float*>(Base + CurrentOffset);
			CurrentOffset += sizeof(float);
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		TaperValues = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float) * TaperCount;
	}

	SourceModifier = (BeamInst && BeamInst->BeamModule_SourceModifier_Offset != INDEX_NONE)
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(Base + BeamInst->BeamModule_SourceModifier_Offset)
		: nullptr;
	TargetModifier = (BeamInst && BeamInst->BeamModule_TargetModifier_Offset != INDEX_NONE)
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(Base + BeamInst->BeamModule_TargetModifier_Offset)
		: nullptr;
}

void UParticleModuleTypeDataBeam2::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, int32& BeamDataOffset, int32& InterpolatedPointsOffset, int32& NoiseRateOffset,
	int32& NoiseDeltaTimeOffset, int32& TargetNoisePointsOffset, int32& NextNoisePointsOffset,
	int32& TaperCount, int32& TaperValuesOffset, int32& NoiseDistanceScaleOffset)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	UParticleModuleBeamNoise* BeamNoise = BeamInst ? BeamInst->BeamModule_Noise : nullptr;
	int32 LocalOffset = 0;

	BeamDataOffset = CurrentOffset + LocalOffset;
	LocalOffset += sizeof(FBeam2TypeDataPayload);

	InterpolatedPointsOffset = INDEX_NONE;
	NoiseRateOffset = INDEX_NONE;
	NoiseDeltaTimeOffset = INDEX_NONE;
	TargetNoisePointsOffset = INDEX_NONE;
	NextNoisePointsOffset = INDEX_NONE;
	TaperValuesOffset = INDEX_NONE;
	NoiseDistanceScaleOffset = INDEX_NONE;
	TaperCount = 2;

	if (InterpolationPoints > 0)
	{
		InterpolatedPointsOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		NoiseRateOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float);
		NoiseDeltaTimeOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float);

		TargetNoisePointsOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(FVector) * Frequency;

		if (BeamNoise->bSmooth)
		{
			NextNoisePointsOffset = CurrentOffset + LocalOffset;
			LocalOffset += sizeof(FVector) * Frequency;
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			NoiseDistanceScaleOffset = CurrentOffset + LocalOffset;
			LocalOffset += sizeof(float);
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		TaperValuesOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float) * TaperCount;
	}

	CurrentOffset += LocalOffset;
}

void UParticleModuleTypeDataBeam2::Spawn(const FSpawnContext& Context)
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
	GetDataPointers(&Context.Owner, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, BeamData, InterpolatedPoints,
		NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints, TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	if (!BeamData)
	{
		return;
	}

	const FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<const FParticleBeam2EmitterInstance*>(&Context.Owner);
	const bool bHasSourceModule = BeamInst && BeamInst->BeamModule_Source;
	const bool bHasTargetModule = BeamInst && BeamInst->BeamModule_Target;

	if (!bHasSourceModule)
	{
		BeamData->SourcePoint = GetBeamEmitterLocation(Context.Owner);
		BeamData->SourceTangent = GetBeamEmitterXAxis(Context.Owner);
		BeamData->SourceStrength = 1.0f;
	}

	if (!bHasTargetModule && BeamMethod == PEB2M_Distance)
	{
		const float BeamDistance = Distance.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		FVector Direction = GetBeamEmitterXAxis(Context.Owner);
		BeamData->TargetPoint = BeamData->SourcePoint + Direction * BeamDistance;
		BeamData->TargetTangent = -Direction;
		BeamData->TargetStrength = 1.0f;
	}

	BeamData->Lock_Max_NumNoisePoints = 0;
	if (BeamInst && BeamInst->BeamModule_Noise && BeamInst->BeamModule_Noise->bLowFreq_Enabled)
	{
		BEAM2_TYPEDATA_SETFREQUENCY(BeamData->Lock_Max_NumNoisePoints, std::max(0, BeamInst->BeamModule_Noise->Frequency));
	}

	const bool bSourceTangentAbsolute = BeamInst && BeamInst->BeamModule_SourceModifier ? BeamInst->BeamModule_SourceModifier->bAbsoluteTangent : false;
	const bool bTargetTangentAbsolute = BeamInst && BeamInst->BeamModule_TargetModifier ? BeamInst->BeamModule_TargetModifier->bAbsoluteTangent : false;
	if (SourceModifier)
	{
		SourceModifier->UpdatePosition(BeamData->SourcePoint);
		SourceModifier->UpdateTangent(BeamData->SourceTangent, bSourceTangentAbsolute);
		SourceModifier->UpdateStrength(BeamData->SourceStrength);
	}
	if (TargetModifier)
	{
		TargetModifier->UpdatePosition(BeamData->TargetPoint);
		TargetModifier->UpdateTangent(BeamData->TargetTangent, bTargetTangentAbsolute);
		TargetModifier->UpdateStrength(BeamData->TargetStrength);
	}

	if (TaperValues && TaperMethod != PEBTM_None)
	{
		int32 TaperCount = InterpolationPoints ? (InterpolationPoints + 1) : 2;
		if (BeamInst && BeamInst->BeamModule_Noise && BeamInst->BeamModule_Noise->bLowFreq_Enabled)
		{
			const int32 Freq = BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints);
			const int32 NoiseTessellation = BeamInst->BeamModule_Noise->NoiseTessellation ? BeamInst->BeamModule_Noise->NoiseTessellation : 1;
			TaperCount = (Freq + 1) * NoiseTessellation;
		}

		const float Increment = TaperCount > 1 ? 1.0f / static_cast<float>(TaperCount - 1) : 0.0f;
		for (int32 TaperIndex = 0; TaperIndex < TaperCount; ++TaperIndex)
		{
			const float CurrStep = static_cast<float>(TaperIndex) * Increment;
			TaperValues[TaperIndex] =
				TaperFactor.GetValue(CurrStep, Context.GetDistributionData()) *
				TaperScale.GetValue(CurrStep, Context.GetDistributionData());
		}
	}

}

void UParticleModuleTypeDataBeam2::Update(const FUpdateContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	UParticleModuleBeamNoise* BeamNoise = BeamInst ? BeamInst->BeamModule_Noise : nullptr;
	UParticleModuleBeamTarget* BeamTarget = BeamInst ? BeamInst->BeamModule_Target : nullptr;
	const bool bSourceTangentAbsolute = BeamInst && BeamInst->BeamModule_SourceModifier ? BeamInst->BeamModule_SourceModifier->bAbsoluteTangent : false;
	const bool bTargetTangentAbsolute = BeamInst && BeamInst->BeamModule_TargetModifier ? BeamInst->BeamModule_TargetModifier->bAbsoluteTangent : false;
	const float LockRadius = BeamTarget ? std::max(0.0f, BeamTarget->LockRadius) : 1.0f;

	BEGIN_UPDATE_LOOP;
	int32 LocalOffset = Context.Owner.TypeDataOffset;
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
	GetDataPointers(&Context.Owner, ParticleBase, LocalOffset, BeamData, InterpolatedPoints,
		NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints, TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	if (!BeamData)
	{
		continue;
	}

	if (BeamInst && !BeamInst->BeamModule_Source)
	{
		BeamData->SourcePoint = GetBeamEmitterLocation(Context.Owner);
		BeamData->SourceTangent = GetBeamEmitterXAxis(Context.Owner);
	}

	if (BeamInst && !BeamInst->BeamModule_Target && BeamInst->BeamMethod == PEB2M_Distance)
	{
		const float TotalDistance = Distance.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		FVector Direction = GetBeamEmitterXAxis(Context.Owner);
		BeamData->TargetPoint = BeamData->SourcePoint + Direction * TotalDistance;
		BeamData->TargetTangent = -Direction;
	}

	if (SourceModifier)
	{
		SourceModifier->UpdatePosition(BeamData->SourcePoint);
		SourceModifier->UpdateTangent(BeamData->SourceTangent, bSourceTangentAbsolute);
		SourceModifier->UpdateStrength(BeamData->SourceStrength);
	}
	if (TargetModifier)
	{
		TargetModifier->UpdatePosition(BeamData->TargetPoint);
		TargetModifier->UpdateTangent(BeamData->TargetTangent, bTargetTangentAbsolute);
		TargetModifier->UpdateStrength(BeamData->TargetStrength);
	}

	if (Speed != 0.0f && !BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
	{
		const FVector ToTarget = BeamData->TargetPoint - Particle.Location;
		FVector MoveDirection = ToTarget.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		const FVector Sum = Particle.Location + MoveDirection * Speed * Context.DeltaTime;
		if (std::fabs(Sum.X - BeamData->TargetPoint.X) < LockRadius &&
			std::fabs(Sum.Y - BeamData->TargetPoint.Y) < LockRadius &&
			std::fabs(Sum.Z - BeamData->TargetPoint.Z) < LockRadius)
		{
			Particle.Location = BeamData->TargetPoint;
			BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, true);
		}
		else
		{
			Particle.Location = Sum;
		}
	}
	else
	{
		Particle.Location = BeamData->TargetPoint;
		BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, true);
	}

	BeamData->Direction = BeamData->TargetPoint - BeamData->SourcePoint;
	const double FullMagnitude = std::max(static_cast<double>(BeamData->Direction.Length()), 0.001);
	BeamData->Direction.Normalize();

	const int32 InterpolationCount = InterpolationPoints > 0 ? InterpolationPoints : 1;
	const bool bLowFreqNoise = BeamNoise && BeamNoise->bLowFreq_Enabled;
	int32 InterpSteps = 0;

	if (!bLowFreqNoise)
	{
		if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
		{
			BeamData->StepSize = FullMagnitude / static_cast<double>(InterpolationCount);
			BeamData->Steps = InterpolationCount;
			BeamData->TravelRatio = 0.0f;
		}
		else
		{
			double TrueMagnitude = static_cast<double>((Particle.Location - BeamData->SourcePoint).Length());
			if (TrueMagnitude > FullMagnitude)
			{
				Particle.Location = BeamData->TargetPoint;
				TrueMagnitude = FullMagnitude;
				BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, true);
				BeamData->StepSize = FullMagnitude / static_cast<double>(InterpolationCount);
				BeamData->Steps = InterpolationCount;
				BeamData->TravelRatio = 0.0f;
			}
			else
			{
				BeamData->StepSize = FullMagnitude / static_cast<double>(InterpolationCount);
				BeamData->TravelRatio = static_cast<float>(TrueMagnitude / FullMagnitude);
				BeamData->Steps = static_cast<int32>(std::floor(BeamData->TravelRatio * static_cast<float>(InterpolationCount)));
				BeamData->TravelRatio = static_cast<float>((TrueMagnitude - (BeamData->StepSize * static_cast<double>(BeamData->Steps))) / BeamData->StepSize);
			}
		}
		InterpSteps = BeamData->Steps;
	}
	else
	{
		InterpSteps = InterpolationCount;
		const int32 Freq = std::max(0, BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints));
		const double TrueMagnitude = static_cast<double>((Particle.Location - BeamData->SourcePoint).Length());
		int32 Count = Freq;
		if (BeamNoise->FrequencyDistance > 0.0f)
		{
			Count = std::min(Freq, static_cast<int32>(FullMagnitude / BeamNoise->FrequencyDistance));
		}

		BeamData->StepSize = FullMagnitude / static_cast<double>(Count + 1);
		if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
		{
			BeamData->Steps = Count;
			BeamData->TravelRatio = 0.0f;
		}
		else
		{
			BeamData->TravelRatio = static_cast<float>(TrueMagnitude / FullMagnitude);
			BeamData->Steps = static_cast<int32>(std::floor(BeamData->TravelRatio * static_cast<float>(Count + 1)));
			BeamData->Steps = std::min(BeamData->Steps, Count);
			const double StepDistance = BeamData->StepSize * static_cast<double>(BeamData->Steps);
			const double Denom = (BeamData->Steps == Count) ? (FullMagnitude - StepDistance) : BeamData->StepSize;
			BeamData->TravelRatio = Denom > 0.001 ? static_cast<float>((TrueMagnitude - StepDistance) / Denom) : 0.0f;
		}

		if (NoiseDistanceScale)
		{
			if (BeamNoise->FrequencyDistance > 0.0f && Freq > 0)
			{
				const float Delta = static_cast<float>(Count) / static_cast<float>(Freq);
				*NoiseDistanceScale = BeamNoise->NoiseScale.GetValue(Delta, Context.GetDistributionData());
			}
			else
			{
				*NoiseDistanceScale = 1.0f;
			}
		}
	}

	if (InterpolatedPoints && InterpolationPoints > 0)
	{
		BeamData->InterpolationSteps = InterpSteps;
		FVector SourceTangent = BeamData->SourceTangent.IsNearlyZero() ? FVector::XAxisVector : BeamData->SourceTangent;
		FVector TargetTangent = BeamData->TargetTangent.IsNearlyZero() ? FVector::XAxisVector : BeamData->TargetTangent;
		SourceTangent *= BeamData->SourceStrength;
		TargetTangent *= BeamData->TargetStrength;
		const float InvTess = 1.0f / static_cast<float>(InterpolationPoints);
		int32 InterpIndex = 0;
		for (; InterpIndex < std::min(InterpSteps, InterpolationPoints); ++InterpIndex)
		{
			InterpolatedPoints[InterpIndex] = CubicInterpVector(
				BeamData->SourcePoint,
				SourceTangent,
				BeamData->TargetPoint,
				TargetTangent,
				InvTess * static_cast<float>(InterpIndex + 1));
		}
		for (; InterpIndex < InterpolationPoints; ++InterpIndex)
		{
			InterpolatedPoints[InterpIndex] = CubicInterpVector(
				BeamData->SourcePoint,
				SourceTangent,
				BeamData->TargetPoint,
				TargetTangent,
				InvTess * static_cast<float>(InterpIndex + 1));
		}

		BeamData->TriangleCount = BeamData->Steps * 2;
		if (bLowFreqNoise)
		{
			const int32 NoiseTess = BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1;
			BeamData->TriangleCount = BeamData->Steps * NoiseTess * 2;
			if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
			{
				BeamData->TriangleCount += NoiseTess * 2;
			}
			else if (BeamData->TravelRatio > 1.0e-4f)
			{
				BeamData->TriangleCount += static_cast<int32>(std::floor(BeamData->TravelRatio * static_cast<float>(NoiseTess))) * 2;
			}
		}
	}
	else
	{
		BeamData->InterpolationSteps = 0;
		if (!bLowFreqNoise)
		{
			BeamData->TriangleCount = 2;
		}
		else
		{
			const int32 NoiseTess = BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1;
			BeamData->TriangleCount = BeamData->Steps * NoiseTess * 2;
			if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
			{
				BeamData->TriangleCount += NoiseTess * 2;
			}
		}
	}

	END_UPDATE_LOOP;
}

FParticleEmitterInstance* UParticleModuleTypeDataBeam2::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent)
{
	return new FParticleBeam2EmitterInstance();
}

void UParticleModuleTypeDataBeam2::CacheModuleInfo(UParticleEmitter* Emitter)
{
	LOD_BeamModule_Source.clear();
	LOD_BeamModule_Target.clear();
	LOD_BeamModule_Noise.clear();
	LOD_BeamModule_SourceModifier.clear();
	LOD_BeamModule_TargetModifier.clear();

	if (!Emitter)
	{
		return;
	}

	for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
	{
		UParticleModuleBeamSource* SourceModule = nullptr;
		UParticleModuleBeamTarget* TargetModule = nullptr;
		UParticleModuleBeamNoise* NoiseModule = nullptr;
		UParticleModuleBeamModifier* SourceModifier = nullptr;
		UParticleModuleBeamModifier* TargetModifier = nullptr;
		if (LODLevel)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				const bool bIsSource = Cast<UParticleModuleBeamSource>(Module) != nullptr;
				const bool bIsTarget = Cast<UParticleModuleBeamTarget>(Module) != nullptr;
				const bool bIsNoise = Cast<UParticleModuleBeamNoise>(Module) != nullptr;
				const bool bIsModifier = Cast<UParticleModuleBeamModifier>(Module) != nullptr;
				const bool bBeamModule = bIsSource || bIsTarget || bIsNoise || bIsModifier;

				if (bIsSource) SourceModule = Cast<UParticleModuleBeamSource>(Module);
				if (bIsTarget) TargetModule = Cast<UParticleModuleBeamTarget>(Module);
				if (bIsNoise) NoiseModule = Cast<UParticleModuleBeamNoise>(Module);
				if (auto* Modifier = Cast<UParticleModuleBeamModifier>(Module))
				{
					if (Modifier->ModifierType == PEB2MT_Source) SourceModifier = Modifier;
					else if (Modifier->ModifierType == PEB2MT_Target) TargetModifier = Modifier;
				}

				if (bBeamModule)
				{
					LODLevel->SpawnModules.erase(
						std::remove(LODLevel->SpawnModules.begin(), LODLevel->SpawnModules.end(), Module),
						LODLevel->SpawnModules.end());
					LODLevel->UpdateModules.erase(
						std::remove(LODLevel->UpdateModules.begin(), LODLevel->UpdateModules.end(), Module),
						LODLevel->UpdateModules.end());
				}
			}
		}
		LOD_BeamModule_Source.push_back(SourceModule);
		LOD_BeamModule_Target.push_back(TargetModule);
		LOD_BeamModule_Noise.push_back(NoiseModule);
		LOD_BeamModule_SourceModifier.push_back(SourceModifier);
		LOD_BeamModule_TargetModifier.push_back(TargetModifier);
	}
}

void UParticleModuleTypeDataBeam2::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
	NoiseMin = FVector::ZeroVector;
	NoiseMax = FVector::ZeroVector;
}

void UParticleModuleTypeDataBeam2::Serialize(FArchive& Ar)
{
	UParticleModuleTypeDataBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(BeamMethod);
	bool AlwaysOn = bAlwaysOn;
	Ar << TextureTile << TextureTileDistance << Sheets << MaxBeamCount << Speed << InterpolationPoints << AlwaysOn << UpVectorStepSize;
	Ar << BranchParentName;
	Distance.Serialize(Ar);
	Ar << reinterpret_cast<int32&>(TaperMethod);
	TaperFactor.Serialize(Ar);
	TaperScale.Serialize(Ar);
	bool RenderGeometryFlag = RenderGeometry;
	bool RenderDirectLineFlag = RenderDirectLine;
	bool RenderLinesFlag = RenderLines;
	bool RenderTessellationFlag = RenderTessellation;
	Ar << RenderGeometryFlag << RenderDirectLineFlag << RenderLinesFlag << RenderTessellationFlag;
	if (Ar.IsLoading())
	{
		bAlwaysOn = AlwaysOn;
		RenderGeometry = RenderGeometryFlag;
		RenderDirectLine = RenderDirectLineFlag;
		RenderLines = RenderLinesFlag;
		RenderTessellation = RenderTessellationFlag;
	}
}
