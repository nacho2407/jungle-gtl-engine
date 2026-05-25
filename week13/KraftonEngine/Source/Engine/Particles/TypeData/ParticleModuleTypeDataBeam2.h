#pragma once

#include "Distributions/DistributionFloat.h"
#include "Object/FName.h"
#include "Particles/ParticleHelper.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;
class UParticleModuleBeamModifier;

UENUM()
enum EBeam2Method : int
{
	PEB2M_Distance,
	PEB2M_Target,
	PEB2M_Branch,
	PEB2M_MAX,
};

UENUM()
enum EBeamTaperMethod : int
{
	PEBTM_None,
	PEBTM_Full,
	PEBTM_Partial,
	PEBTM_MAX,
};

struct FBeamTargetData
{
	FName TargetName = FName::None;
	float TargetPercentage = 0.0f;
};

#include "Source/Engine/Particles/TypeData/ParticleModuleTypeDataBeam2.generated.h"

UCLASS()
class UParticleModuleTypeDataBeam2 : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()

	EBeam2Method BeamMethod = PEB2M_Distance;
	int32 TextureTile = 1;
	float TextureTileDistance = 0.0f;
	int32 Sheets = 1;
	int32 MaxBeamCount = 1;
	float Speed = 0.0f;
	int32 InterpolationPoints = 0;
	uint32 bAlwaysOn : 1;
	int32 UpVectorStepSize = 0;
	FName BranchParentName = FName::None;
	FRawDistributionFloat Distance;
	EBeamTaperMethod TaperMethod = PEBTM_None;
	FRawDistributionFloat TaperFactor;
	FRawDistributionFloat TaperScale;
	uint32 RenderGeometry : 1;
	uint32 RenderDirectLine : 1;
	uint32 RenderLines : 1;
	uint32 RenderTessellation : 1;

	TArray<UParticleModuleBeamSource*> LOD_BeamModule_Source;
	TArray<UParticleModuleBeamTarget*> LOD_BeamModule_Target;
	TArray<UParticleModuleBeamNoise*> LOD_BeamModule_Noise;
	TArray<UParticleModuleBeamModifier*> LOD_BeamModule_SourceModifier;
	TArray<UParticleModuleBeamModifier*> LOD_BeamModule_TargetModifier;

	UParticleModuleTypeDataBeam2();
	void InitializeDefaults();

	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent) override;
	void CacheModuleInfo(UParticleEmitter* Emitter) override;

	void GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset, FBeam2TypeDataPayload*& BeamData, FVector*& InterpolatedPoints,
		float*& NoiseRate, float*& NoiseDeltaTime, FVector*& TargetNoisePoints,
		FVector*& NextNoisePoints, float*& TaperValues, float*& NoiseDistanceScale,
		FBeamParticleModifierPayloadData*& SourceModifier,
		FBeamParticleModifierPayloadData*& TargetModifier);

	void GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
		int32& CurrentOffset, int32& BeamDataOffset, int32& InterpolatedPointsOffset, int32& NoiseRateOffset,
		int32& NoiseDeltaTimeOffset, int32& TargetNoisePointsOffset, int32& NextNoisePointsOffset,
		int32& TaperCount, int32& TaperValuesOffset, int32& NoiseDistanceScaleOffset);

	void GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax);
};
