#include "Particles/Beam/ParticleModuleBeamSource.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Serialization/Archive.h"

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
	if (SourceMethod == PEB2STM_Emitter)
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
}

void UParticleModuleBeamSource::Update(const FUpdateContext& Context)
{
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
	if (SourceMethod == PEB2STM_Emitter)
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
	if (SourceMethod == PEB2STM_Emitter)
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

	// UE resolves Actor / Emitter / Particle source methods through named
	// component parameters, emitter instance lookup, and selected source
	// particles. Jungle does not expose those foundations yet, so these methods
	// are intentionally stubbed. Do not fall back to Default distribution,
	// Owner.Location, or any substitute source.
	if (SourceMethod == PEB2STM_Actor ||
		SourceMethod == PEB2STM_Emitter ||
		SourceMethod == PEB2STM_Particle)
	{
		return false;
	}

	switch (SourceMethod)
	{
	case PEB2STM_UserSet:
		BeamInst->GetBeamSourcePoint(ParticleIndex, BeamData->SourcePoint);
		BeamInst->GetBeamSourceTangent(ParticleIndex, BeamData->SourceTangent);
		BeamInst->GetBeamSourceStrength(ParticleIndex, BeamData->SourceStrength);
		break;
	case PEB2STM_Default:
		BeamData->SourcePoint = Source.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		BeamData->SourceTangent = SourceTangent.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		BeamData->SourceStrength = SourceStrength.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		break;
	default:
		return false;
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
