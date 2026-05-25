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
struct FBeamParticleSourceBranchPayloadData;

#include "Source/Engine/Particles/Beam/ParticleModuleBeamSource.generated.h"

UCLASS()
class UParticleModuleBeamSource : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	Beam2SourceTargetMethod SourceMethod = PEB2STM_Default;
	FName SourceName = FName::None;
	uint32 bSourceAbsolute : 1;
	FRawDistributionVector Source;
	uint32 bLockSource : 1;
	Beam2SourceTargetTangentMethod SourceTangentMethod = PEB2STTM_Direct;
	FRawDistributionVector SourceTangent;
	uint32 bLockSourceTangent : 1;
	FRawDistributionFloat SourceStrength;
	uint32 bLockSourceStrength : 1;
	int32 LastSelectedParticleIndex = INDEX_NONE;

	UParticleModuleBeamSource();
	void InitializeDefaults();
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void Serialize(FArchive& Ar) override;

	void GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset,
		FBeamParticleSourceTargetPayloadData*& ParticleSource,
		FBeamParticleSourceBranchPayloadData*& BranchSource);

	void GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset, int32& ParticleSourceOffset, int32& BranchSourceOffset);

	bool ResolveSourceData(const FContext& Context, FParticleBeam2EmitterInstance* BeamInst,
		FBeam2TypeDataPayload* BeamData, const uint8* ParticleBase,
		int32& CurrentOffset, int32 ParticleIndex, bool bSpawning,
		FBeamParticleModifierPayloadData* ModifierData);
};
