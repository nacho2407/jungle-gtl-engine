#include "Particles/Beam/ParticleModuleBeamSource.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Serialization/Archive.h"

UParticleModuleBeamSource::UParticleModuleBeamSource()
	: bSourceAbsolute(false)
	, bLockSource(false)
	, bLockSourceTangent(false)
	, bLockSourceStength(false)
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
	if (!BeamInst)
	{
		return;
	}

	FBeam2TypeDataPayload* BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(reinterpret_cast<uint8*>(Context.ParticleBase) + Context.Owner.TypeDataOffset);
	int32 CurrentOffset = Context.Offset;
	ResolveSourceData(Context, BeamInst, BeamData, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, Context.Owner.ActiveParticles, true, nullptr);
}

void UParticleModuleBeamSource::Update(const FUpdateContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst)
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	FBeam2TypeDataPayload* BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(ParticleBase + Context.Owner.TypeDataOffset);
	int32 LocalOffset = Context.Offset;
	ResolveSourceData(Context, BeamInst, BeamData, ParticleBase, LocalOffset, i, false, nullptr);
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
	case PEB2STM_Actor:
	case PEB2STM_Emitter:
	case PEB2STM_Particle:
		// UE original responsibility: resolve Actor/Emitter/Particle/Name sources.
		// Missing Jungle foundation: actor lookup, emitter-name lookup, particle source selection, branch beam.
		// System to connect later: particle system component instance parameters and emitter-instance lookup table.
		break;
	default:
		break;
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
	bool LockSourceStrength = bLockSourceStength;
	Ar << LockSourceStrength;
	if (Ar.IsLoading())
	{
		bSourceAbsolute = SourceAbsolute;
		bLockSource = LockSource;
		bLockSourceTangent = LockSourceTangent;
		bLockSourceStength = LockSourceStrength;
	}
}
