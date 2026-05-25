#include "Particles/Beam/ParticleModuleBeamSource.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Serialization/Archive.h"

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

	FVector GetBeamEmitterLocation(const FParticleBeam2EmitterInstance* BeamInst)
	{
		return (BeamInst && BeamInst->Component) ? BeamInst->Component->GetWorldLocation() : FVector::ZeroVector;
	}
}

UParticleModuleBeamSource::UParticleModuleBeamSource()
	: bSourceAbsolute(false)
	, bLockSource(false)
	, bLockSourceTangent(false)
	, bLockSourceStrength(false)
{
	InitializeDefaults();
}

void UParticleModuleBeamSource::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
	SourceMethod = PEB2STM_Default;
	SourceTangentMethod = PEB2STTM_Direct;
}

uint32 UParticleModuleBeamSource::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	uint32 Size = 0;
	if (SourceMethod == PEB2STM_Particle)
	{
		Size += sizeof(FBeamParticleSourceTargetPayloadData);
	}
	UParticleModuleTypeDataBeam2* BeamTD = Cast<UParticleModuleTypeDataBeam2>(TypeData);
	if (BeamTD && BeamTD->BeamMethod == PEB2M_Branch)
	{
		Size += sizeof(FBeamParticleSourceBranchPayloadData);
	}
	return Size;
}

void UParticleModuleBeamSource::Spawn(const FSpawnContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData)
	{
		return;
	}

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
	ResolveSourceData(Context, BeamInst, BeamData, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, Context.Owner.ActiveParticles, true, SourceModifier);

	if (BeamData)
	{
		Context.ParticleBase->Location = BeamData->SourcePoint - BeamInst->PositionOffsetThisTick;
		BeamData->Lock_Max_NumNoisePoints = 0;
		BeamData->StepSize = 0.0;
		BeamData->Steps = 0;
		BeamData->TravelRatio = 0.0f;
		BeamData->TriangleCount = 0;
	}
}

void UParticleModuleBeamSource::Update(const FUpdateContext& Context)
{
	if (bLockSource && bLockSourceTangent && bLockSourceStrength)
	{
		return;
	}

	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst || !BeamInst->BeamTypeData)
	{
		return;
	}

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
	ResolveSourceData(Context, BeamInst, BeamData, ParticleBase, LocalOffset, i, false, SourceModifier);
	END_UPDATE_LOOP;
}

void UParticleModuleBeamSource::GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset,
	FBeamParticleSourceTargetPayloadData*& ParticleSource,
	FBeamParticleSourceBranchPayloadData*& BranchSource)
{
	ParticleSource = nullptr;
	BranchSource = nullptr;
	if (SourceMethod == PEB2STM_Particle)
	{
		ParticleSource = reinterpret_cast<FBeamParticleSourceTargetPayloadData*>(const_cast<uint8*>(ParticleBase) + CurrentOffset);
		CurrentOffset += sizeof(FBeamParticleSourceTargetPayloadData);
	}
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	if (BeamInst && BeamInst->BeamTypeData && BeamInst->BeamTypeData->BeamMethod == PEB2M_Branch)
	{
		BranchSource = reinterpret_cast<FBeamParticleSourceBranchPayloadData*>(const_cast<uint8*>(ParticleBase) + CurrentOffset);
		CurrentOffset += sizeof(FBeamParticleSourceBranchPayloadData);
	}
}

void UParticleModuleBeamSource::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, int32& ParticleSourceOffset, int32& BranchSourceOffset)
{
	ParticleSourceOffset = INDEX_NONE;
	BranchSourceOffset = INDEX_NONE;
	if (SourceMethod == PEB2STM_Particle)
	{
		ParticleSourceOffset = CurrentOffset;
		CurrentOffset += sizeof(FBeamParticleSourceTargetPayloadData);
	}
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	if (BeamInst && BeamInst->BeamTypeData && BeamInst->BeamTypeData->BeamMethod == PEB2M_Branch)
	{
		BranchSourceOffset = CurrentOffset;
		CurrentOffset += sizeof(FBeamParticleSourceBranchPayloadData);
	}
}

bool UParticleModuleBeamSource::ResolveSourceData(const FContext& Context, FParticleBeam2EmitterInstance* BeamInst,
	FBeam2TypeDataPayload* BeamData, const uint8* ParticleBase,
	int32& CurrentOffset, int32 ParticleIndex, bool bSpawning,
	FBeamParticleModifierPayloadData* ModifierData)
{
	if (!BeamData)
	{
		return false;
	}
	FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(const_cast<uint8*>(ParticleBase));

	// UE resolves Actor / Particle source methods through named component
	// parameters, emitter instance lookup, and selected source particles.
	// Jungle does not expose those foundations yet, so these methods are
	// intentionally stubbed. Do not fall back to Default distribution,
	// Owner.Location, or any substitute source.
	if (SourceMethod == PEB2STM_Actor ||
		SourceMethod == PEB2STM_Particle)
	{
		return false;
	}

	switch (SourceMethod)
	{
	case PEB2STM_UserSet:
		if (bSpawning || !bLockSource)
		{
			if (!BeamInst->GetBeamSourcePoint(ParticleIndex, BeamData->SourcePoint))
			{
				BeamInst->GetBeamSourcePoint(0, BeamData->SourcePoint);
			}
		}
		break;
	case PEB2STM_Emitter:
		if (bSpawning || !bLockSource) BeamData->SourcePoint = GetBeamEmitterLocation(BeamInst);
		break;
	case PEB2STM_Default:
		if (bSpawning || !bLockSource) BeamData->SourcePoint = Source.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		break;
	default:
		return false;
	}

	if (bSpawning || !bLockSourceTangent)
	{
		bool bSetSourceTangent = false;
		switch (SourceTangentMethod)
		{
		case PEB2STTM_Direct:
		case PEB2STTM_Emitter:
			BeamData->SourceTangent = GetBeamEmitterXAxis(BeamInst);
			bSetSourceTangent = true;
			break;
		case PEB2STTM_UserSet:
			if (BeamInst->GetBeamSourceTangent(ParticleIndex, BeamData->SourceTangent) ||
				BeamInst->GetBeamSourceTangent(0, BeamData->SourceTangent))
			{
				bSetSourceTangent = true;
			}
			break;
		case PEB2STTM_Distribution:
			BeamData->SourceTangent = SourceTangent.GetValue(Particle.RelativeTime, Context.GetDistributionData());
			bSetSourceTangent = true;
			break;
		}

		if (!bSetSourceTangent)
		{
			BeamData->SourceTangent = SourceTangent.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		}
	}

	if (bSpawning || !bLockSourceStrength)
	{
		bool bSetSourceStrength = false;
		if (SourceTangentMethod == PEB2STTM_UserSet)
		{
			if (BeamInst->GetBeamSourceStrength(ParticleIndex, BeamData->SourceStrength) ||
				BeamInst->GetBeamSourceStrength(0, BeamData->SourceStrength))
			{
				bSetSourceStrength = true;
			}
		}
		if (!bSetSourceStrength)
		{
			BeamData->SourceStrength = SourceStrength.GetValue(Particle.RelativeTime, Context.GetDistributionData());
		}
	}

	return true;
}

void UParticleModuleBeamSource::Serialize(FArchive& Ar)
{
	UParticleModuleBeamBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(SourceMethod);
	Ar << SourceName;
	bool SourceAbsolute = bSourceAbsolute;
	Ar << SourceAbsolute;
	Source.Serialize(Ar);
	bool LockSource = bLockSource;
	Ar << LockSource;
	Ar << reinterpret_cast<int32&>(SourceTangentMethod);
	SourceTangent.Serialize(Ar);
	bool LockSourceTangent = bLockSourceTangent;
	Ar << LockSourceTangent;
	SourceStrength.Serialize(Ar);
	bool LockSourceStrength = bLockSourceStrength;
	Ar << LockSourceStrength;
	if (Ar.IsLoading())
	{
		bSourceAbsolute = SourceAbsolute;
		bLockSource = LockSource;
		bLockSourceTangent = LockSourceTangent;
		bLockSourceStrength = LockSourceStrength;
	}
}
