#include "Particles/Beam/ParticleModuleBeamTarget.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleBeamTarget::UParticleModuleBeamTarget()
	: bTargetAbsolute(false)
	, bLockTarget(false)
	, bLockTargetTangent(false)
	, bLockTargetStength(false)
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

void UParticleModuleBeamTarget::Spawn(const FSpawnContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst) return;
	FBeam2TypeDataPayload* BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(reinterpret_cast<uint8*>(Context.ParticleBase) + Context.Owner.TypeDataOffset);
	int32 CurrentOffset = Context.Offset;
	ResolveTargetData(Context, BeamInst, BeamData, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, Context.Owner.ActiveParticles, true, nullptr);
}

void UParticleModuleBeamTarget::Update(const FUpdateContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst) return;
	BEGIN_UPDATE_LOOP;
	FBeam2TypeDataPayload* BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(ParticleBase + Context.Owner.TypeDataOffset);
	int32 LocalOffset = Context.Offset;
	ResolveTargetData(Context, BeamInst, BeamData, ParticleBase, LocalOffset, i, false, nullptr);
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
	switch (TargetMethod)
	{
	case PEB2STM_UserSet:
		BeamInst->GetBeamTargetPoint(ParticleIndex, BeamData->TargetPoint);
		BeamInst->GetBeamTargetTangent(ParticleIndex, BeamData->TargetTangent);
		BeamInst->GetBeamTargetStrength(ParticleIndex, BeamData->TargetStrength);
		break;
	case PEB2STM_Default:
		BeamData->TargetPoint = Target.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		BeamData->TargetTangent = TargetTangent.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		BeamData->TargetStrength = TargetStrength.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		break;
	case PEB2STM_Actor:
	case PEB2STM_Emitter:
	case PEB2STM_Particle:
		// UE original responsibility: resolve Actor/Emitter/Particle/Name targets.
		// Missing Jungle foundation: actor lookup, emitter-name lookup, particle source selection, branch beam.
		// System to connect later: particle system component instance parameters and emitter-instance lookup table.
		break;
	default:
		break;
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
	bool LockTargetStrength = bLockTargetStength;
	Ar << LockTargetStrength;
	Ar << LockRadius;
	if (Ar.IsLoading())
	{
		bTargetAbsolute = TargetAbsolute;
		bLockTarget = LockTarget;
		bLockTargetTangent = LockTargetTangent;
		bLockTargetStength = LockTargetStrength;
	}
}
