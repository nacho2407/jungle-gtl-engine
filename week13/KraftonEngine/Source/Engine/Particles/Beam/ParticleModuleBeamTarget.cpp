#include "Particles/Beam/ParticleModuleBeamTarget.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

#include <algorithm>

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

void UParticleModuleBeamTarget::Spawn(const FSpawnContext& Context)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(&Context.Owner);
	if (!BeamInst) return;

	FBeam2TypeDataPayload* BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(reinterpret_cast<uint8*>(Context.ParticleBase) + Context.Owner.TypeDataOffset);
	int32 CurrentOffset = Context.Offset;

	const int32 BeamIndex = std::max(0, Context.Owner.ActiveParticles - 1);
	ResolveTargetData(Context, BeamInst, BeamData, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, BeamIndex, true, nullptr);
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

	// UE resolves Actor / Emitter / Particle target methods through named
	// component parameters, emitter instance lookup, and selected source
	// particles. Jungle does not expose those foundations yet, so these methods
	// are intentionally stubbed. Do not fall back to Default distribution or
	// Owner.Location, because that changes the meaning of the Cascade module.
	if (TargetMethod == PEB2STM_Actor ||
		TargetMethod == PEB2STM_Emitter ||
		TargetMethod == PEB2STM_Particle)
	{
		return false;
	}

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
	default:
		return false;
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
