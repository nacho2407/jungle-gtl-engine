#pragma once

#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

UENUM()
enum ETrailsRenderAxisOption : int
{
	Trails_CameraUp,
	Trails_SourceUp,
	Trails_WorldUp,
	Trails_MAX,
};

#include "Source/Engine/Particles/TypeData/ParticleModuleTypeDataRibbon.generated.h"

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()

	int32 MaxTessellationBetweenParticles = 0;
	int32 SheetsPerTrail = 1;
	int32 MaxTrailCount = 1;
	int32 MaxParticleInTrailCount = 0;
	uint32 bDeadTrailsOnDeactivate : 1;
	uint32 bDeadTrailsOnSourceLoss : 1;
	uint32 bClipSourceSegement : 1;
	uint32 bEnablePreviousTangentRecalculation : 1;
	uint32 bTangentRecalculationEveryFrame : 1;
	uint32 bSpawnInitialParticle : 1;
	ETrailsRenderAxisOption RenderAxis = Trails_CameraUp;
	float TangentSpawningScalar = 0.0f;
	uint32 bRenderGeometry : 1;
	uint32 bRenderSpawnPoints : 1;
	uint32 bRenderTangents : 1;
	uint32 bRenderTessellation : 1;
	float TilingDistance = 0.0f;
	float DistanceTessellationStepSize = 12.5f;
	uint32 bEnableTangentDiffInterpScale : 1;
	float TangentTessellationScalar = 25.0f;

	UParticleModuleTypeDataRibbon();
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent) override;
	void Serialize(FArchive& Ar) override;
};
