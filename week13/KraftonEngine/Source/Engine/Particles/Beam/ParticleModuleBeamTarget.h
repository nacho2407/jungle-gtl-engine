#pragma once

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Object/FName.h"
#include "Particles/ParticleHelper.h"
#include "Particles/Beam/ParticleModuleBeamBase.h"

struct FParticleBeam2EmitterInstance;
struct FBeam2TypeDataPayload;
struct FBeamParticleModifierPayloadData;
struct FBeamParticleSourceTargetPayloadData;

#include "Source/Engine/Particles/Beam/ParticleModuleBeamTarget.generated.h"

UCLASS()
class UParticleModuleBeamTarget : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	Beam2SourceTargetMethod TargetMethod = PEB2STM_Default;
	FName TargetName = FName::None;
	FRawDistributionVector Target;
	uint32 bTargetAbsolute : 1;
	uint32 bLockTarget : 1;
	Beam2SourceTargetTangentMethod TargetTangentMethod = PEB2STTM_Direct;
	FRawDistributionVector TargetTangent;
	uint32 bLockTargetTangent : 1;
	FRawDistributionFloat TargetStrength;
	uint32 bLockTargetStrength : 1;
	float LockRadius = 0.0f;
	int32 LastSelectedParticleIndex = INDEX_NONE;

	UParticleModuleBeamTarget();
	void InitializeDefaults();
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	void Serialize(FArchive& Ar) override;

	void GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset,
		FBeamParticleSourceTargetPayloadData*& ParticleSource);

	bool ResolveTargetData(const FContext& Context, FParticleBeam2EmitterInstance* BeamInst,
		FBeam2TypeDataPayload* BeamData, const uint8* ParticleBase,
		int32& CurrentOffset, int32 ParticleIndex, bool bSpawning,
		FBeamParticleModifierPayloadData* ModifierData);
};
