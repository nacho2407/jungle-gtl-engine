#pragma once

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particles/Beam/ParticleModuleBeamBase.h"

struct FBeam2TypeDataPayload;
struct FBeamParticleModifierPayloadData;

UENUM()
enum BeamModifierType : int
{
	PEB2MT_Source,
	PEB2MT_Target,
	PEB2MT_MAX,
};

struct FBeamModifierOptions
{
	uint32 bModify : 1;
	uint32 bScale : 1;
	uint32 bLock : 1;

	FBeamModifierOptions()
		: bModify(false)
		, bScale(false)
		, bLock(false)
	{
	}
};

#include "Source/Engine/Particles/Beam/ParticleModuleBeamModifier.generated.h"

UCLASS()
class UParticleModuleBeamModifier : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	BeamModifierType ModifierType = PEB2MT_Source;
	FBeamModifierOptions PositionOptions;
	FRawDistributionVector Position;
	FBeamModifierOptions TangentOptions;
	FRawDistributionVector Tangent;
	uint32 bAbsoluteTangent : 1;
	FBeamModifierOptions StrengthOptions;
	FRawDistributionFloat Strength;

	UParticleModuleBeamModifier();
	void InitializeDefaults();
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void Serialize(FArchive& Ar) override;

	void GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset, FBeam2TypeDataPayload*& BeamDataPayload,
		FBeamParticleModifierPayloadData*& SourceModifierPayload,
		FBeamParticleModifierPayloadData*& TargetModifierPayload);

	void GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset, int32& BeamDataOffset, int32& SourceModifierOffset,
		int32& TargetModifierOffset);
};
