#include "Particles/Trail/ParticleModuleTrailSource.h"

#include "Serialization/Archive.h"

UParticleModuleTrailSource::UParticleModuleTrailSource()
	: bLockSourceStrength(false)
	, bInheritRotation(false)
{
	InitializeDefaults();
}

void UParticleModuleTrailSource::InitializeDefaults()
{
	SourceMethod = PET2SRCM_Default;
}

bool UParticleModuleTrailSource::ResolveSourceOffset(int32 InTrailIdx, FParticleEmitterInstance* InEmitterInst, FVector& OutSourceOffset)
{
	switch (SourceMethod)
	{
	case PET2SRCM_Default:
		if (SourceOffsetDefaults.empty())
		{
			return false;
		}
		OutSourceOffset = SourceOffsetDefaults[InTrailIdx % static_cast<int32>(SourceOffsetDefaults.size())];
		return true;

	case PET2SRCM_Particle:
		// UE original responsibility:
		// Resolve SourceName to another emitter and choose a particle using SelectionMethod.
		// Missing Jungle foundation: emitter-name lookup and source particle selection.
		// Keep as stub. Do not fall back to default offsets.
		return false;

	case PET2SRCM_Actor:
		// UE original responsibility:
		// Resolve SourceName to an actor/component instance parameter.
		// Missing Jungle foundation: actor/source parameter lookup.
		// Keep as stub. Do not fall back to default offsets.
		return false;

	default:
		return false;
	}
}

void UParticleModuleTrailSource::Serialize(FArchive& Ar)
{
	UParticleModuleTrailBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(SourceMethod);
	Ar << SourceName;
	SourceStrength.Serialize(Ar);

	bool LockSourceStrength = bLockSourceStrength;
	Ar << LockSourceStrength << SourceOffsetCount;
	Ar << SourceOffsetDefaults;
	Ar << reinterpret_cast<int32&>(SelectionMethod);

	bool InheritRotation = bInheritRotation;
	Ar << InheritRotation;

	if (Ar.IsLoading())
	{
		bLockSourceStrength = LockSourceStrength;
		bInheritRotation = InheritRotation;
	}
}
