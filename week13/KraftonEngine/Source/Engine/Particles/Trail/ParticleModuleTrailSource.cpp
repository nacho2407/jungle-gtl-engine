#include "Particles/Trail/ParticleModuleTrailSource.h"

#include "Serialization/Archive.h"

UParticleModuleTrailSource::UParticleModuleTrailSource()
	: bLockSourceStength(false)
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
	if (SourceOffsetDefaults.empty())
	{
		return false;
	}
	OutSourceOffset = SourceOffsetDefaults[InTrailIdx % static_cast<int32>(SourceOffsetDefaults.size())];
	return true;
}

void UParticleModuleTrailSource::Serialize(FArchive& Ar)
{
	UParticleModuleTrailBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(SourceMethod);
	Ar << SourceName;
	SourceStrength.Serialize(Ar);
	bool LockSourceStrength = bLockSourceStength;
	Ar << LockSourceStrength << SourceOffsetCount;
	Ar << SourceOffsetDefaults;
	Ar << reinterpret_cast<int32&>(SelectionMethod);
	bool InheritRotation = bInheritRotation;
	Ar << InheritRotation;
	if (Ar.IsLoading())
	{
		bLockSourceStength = LockSourceStrength;
		bInheritRotation = InheritRotation;
	}
}
