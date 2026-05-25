#include "Particles/Beam/ParticleModuleBeamTarget.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Serialization/Archive.h"

#include <cmath>

namespace
{
	FVector GetBeamEmitterXAxis(const FParticleBeam2EmitterInstance* BeamInst)
	{
		if (BeamInst && BeamInst->Component)
		{
			return BeamInst->Component->GetWorldMatrix().TransformVector(FVector::XAxisVector).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		}
		return FVector::XAxisVector;
	}
}

UParticleModuleBeamTarget::UParticleModuleBeamTarget()
	: bTargetAbsolute(false)
	, bLockTarget(false)
	, bLockTargetTangent(false)
	, bLockTargetStrength(false)
{
	InitializeDefaults();
}

void UParticleModuleBeamTarget::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
	TargetMethod = PEB2STM_Default;
	TargetTangentMethod = PEB2STTM_Direct;
}

uint32 UParticleModuleBeamTarget::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	uint32 Size = 0;
	if (TargetMethod == PEB2STM_Particle)
	{
		Size += sizeof(FBeamParticleSourceTargetPayloadData);
	}
	return Size;
}

void UParticleModuleBeamTarget::Spawn(const FSpawnContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData) return;

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

	int32 TypeDataCurrentOffset = Context.Owner.TypeDataOffset;
	BeamInst->BeamTypeData->GetDataPointers(&Context.Owner, reinterpret_cast<const uint8*>(Context.ParticleBase), TypeDataCurrentOffset,
		BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	int32 CurrentOffset = Context.Offset;

	// Keep the Unreal module-facing beam index convention.
	// If this engine increments ActiveParticles at a different point,
	// fix SpawnParticles ordering instead of compensating in this module.
	ResolveTargetData(Context, BeamInst, BeamData, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, Context.Owner.ActiveParticles, true, TargetModifier);
}

void UParticleModuleBeamTarget::Update(const FUpdateContext& Context)
{
	if (bLockTarget && bLockTargetTangent && bLockTargetStrength)
	{
		return;
	}

	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData) return;

	BEGIN_UPDATE_LOOP;
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

	int32 TypeDataCurrentOffset = Context.Owner.TypeDataOffset;
	BeamInst->BeamTypeData->GetDataPointers(&Context.Owner, ParticleBase, TypeDataCurrentOffset,
		BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	int32 LocalOffset = Context.Offset;
	ResolveTargetData(Context, BeamInst, BeamData, ParticleBase, LocalOffset, i, false, TargetModifier);
	END_UPDATE_LOOP;
}

void UParticleModuleBeamTarget::GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset,
	FBeamParticleSourceTargetPayloadData*& ParticleSource)
{
	ParticleSource = nullptr;
	if (TargetMethod == PEB2STM_Particle)
	{
		ParticleSource = reinterpret_cast<FBeamParticleSourceTargetPayloadData*>(const_cast<uint8*>(ParticleBase) + CurrentOffset);
		CurrentOffset += sizeof(FBeamParticleSourceTargetPayloadData);
	}
}

bool UParticleModuleBeamTarget::ResolveTargetData(const FContext& Context, FParticleBeam2EmitterInstance* BeamInst,
	FBeam2TypeDataPayload* BeamData, const uint8* ParticleBase,
	int32& CurrentOffset, int32 ParticleIndex, bool bSpawning,
	FBeamParticleModifierPayloadData* ModifierData)
{
	if (!BeamData) return false;
	FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(const_cast<uint8*>(ParticleBase));

	// UE resolves Actor / Emitter / Particle target methods through named
	// component parameters, emitter instance lookup, and selected source
	// particles. Jungle does not expose those foundations yet, so these methods
	// are intentionally stubbed. Do not fall back to Default distribution,
	// Owner.Location, or any substitute target.
	if (TargetMethod == PEB2STM_Actor ||
		TargetMethod == PEB2STM_Emitter ||
		TargetMethod == PEB2STM_Particle)
	{
		return false;
	}

	if (bSpawning || !bLockTarget)
	{
		bool bSetTarget = false;
		if (BeamInst->BeamTypeData && BeamInst->BeamTypeData->BeamMethod == PEB2M_Distance)
		{
			float BeamDistance = BeamInst->BeamTypeData->Distance.GetValue(Particle.RelativeTime, Context.GetDistributionData());
			if (std::fabs(BeamDistance) < 1.0e-4f)
			{
				BeamDistance = 0.001f;
			}
			FVector Direction = GetBeamEmitterXAxis(BeamInst);
			BeamData->TargetPoint = BeamData->SourcePoint + Direction * BeamDistance;
			bSetTarget = true;
		}

		if (!bSetTarget)
		{
			switch (TargetMethod)
			{
			case PEB2STM_UserSet:
				if (BeamInst->GetBeamTargetPoint(ParticleIndex, BeamData->TargetPoint) ||
					BeamInst->GetBeamTargetPoint(0, BeamData->TargetPoint))
				{
					bSetTarget = true;
				}
				break;
			case PEB2STM_Default:
				BeamData->TargetPoint = Target.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
				bSetTarget = true;
				break;
			default:
				return false;
			}
		}
	}

	if (bSpawning || !bLockTargetTangent)
	{
		bool bSetTargetTangent = false;
		switch (TargetTangentMethod)
		{
		case PEB2STTM_Direct:
		case PEB2STTM_Emitter:
			BeamData->TargetTangent = GetBeamEmitterXAxis(BeamInst);
			bSetTargetTangent = true;
			break;
		case PEB2STTM_UserSet:
			if (BeamInst->GetBeamTargetTangent(ParticleIndex, BeamData->TargetTangent) ||
				BeamInst->GetBeamTargetTangent(0, BeamData->TargetTangent))
			{
				bSetTargetTangent = true;
			}
			break;
		case PEB2STTM_Distribution:
			BeamData->TargetTangent = TargetTangent.GetValue(Particle.RelativeTime, Context.GetDistributionData());
			bSetTargetTangent = true;
			break;
		}
		if (!bSetTargetTangent)
		{
			BeamData->TargetTangent = TargetTangent.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		}
	}

	if (bSpawning || !bLockTargetStrength)
	{
		bool bSetTargetStrength = false;
		if (TargetTangentMethod == PEB2STTM_UserSet)
		{
			if (BeamInst->GetBeamTargetStrength(ParticleIndex, BeamData->TargetStrength) ||
				BeamInst->GetBeamTargetStrength(0, BeamData->TargetStrength))
			{
				bSetTargetStrength = true;
			}
		}
		if (!bSetTargetStrength)
		{
			BeamData->TargetStrength = TargetStrength.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		}
	}

	return true;
}

void UParticleModuleBeamTarget::Serialize(FArchive& Ar)
{
	UParticleModuleBeamBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(TargetMethod);
	Ar << TargetName;
	Target.Serialize(Ar);
	bool TargetAbsolute = bTargetAbsolute;
	bool LockTarget = bLockTarget;
	Ar << TargetAbsolute;
	Ar << LockTarget;
	Ar << reinterpret_cast<int32&>(TargetTangentMethod);
	TargetTangent.Serialize(Ar);
	bool LockTargetTangent = bLockTargetTangent;
	Ar << LockTargetTangent;
	TargetStrength.Serialize(Ar);
	bool LockTargetStrength = bLockTargetStrength;
	Ar << LockTargetStrength;
	Ar << LockRadius;
	if (Ar.IsLoading())
	{
		bTargetAbsolute = TargetAbsolute;
		bLockTarget = LockTarget;
		bLockTargetTangent = LockTargetTangent;
		bLockTargetStrength = LockTargetStrength;
	}
}
