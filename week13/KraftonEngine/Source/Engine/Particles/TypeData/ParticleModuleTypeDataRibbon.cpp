#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleTypeDataRibbon::UParticleModuleTypeDataRibbon()
	: bDeadTrailsOnDeactivate(true)
	, bDeadTrailsOnSourceLoss(true)
	, bClipSourceSegement(true)
	, bEnablePreviousTangentRecalculation(true)
	, bTangentRecalculationEveryFrame(false)
	, bSpawnInitialParticle(false)
	, bRenderGeometry(true)
	, bRenderSpawnPoints(false)
	, bRenderTangents(false)
	, bRenderTessellation(false)
	, bEnableTangentDiffInterpScale(false)
{
	MaxTessellationBetweenParticles = 25;
	SheetsPerTrail = 1;
	MaxTrailCount = 1;
	MaxParticleInTrailCount = 500;
	TangentSpawningScalar = 0.0f;
	DistanceTessellationStepSize = 15.0f;
	TangentTessellationScalar = 5.0f;
}

uint32 UParticleModuleTypeDataRibbon::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FRibbonTypeDataPayload);
}

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent)
{
	return new FParticleRibbonEmitterInstance();
}

void UParticleModuleTypeDataRibbon::Serialize(FArchive& Ar)
{
	UParticleModuleTypeDataBase::Serialize(Ar);
	Ar << MaxTessellationBetweenParticles << SheetsPerTrail << MaxTrailCount << MaxParticleInTrailCount;
	bool DeadTrailsOnDeactivate = bDeadTrailsOnDeactivate;
	bool DeadTrailsOnSourceLoss = bDeadTrailsOnSourceLoss;
	bool ClipSourceSegement = bClipSourceSegement;
	Ar << DeadTrailsOnDeactivate << DeadTrailsOnSourceLoss << ClipSourceSegement;
	bool EnablePreviousTangentRecalculation = bEnablePreviousTangentRecalculation;
	bool TangentRecalculationEveryFrame = bTangentRecalculationEveryFrame;
	bool SpawnInitialParticle = bSpawnInitialParticle;
	Ar << EnablePreviousTangentRecalculation << TangentRecalculationEveryFrame << SpawnInitialParticle;
	Ar << reinterpret_cast<int32&>(RenderAxis);
	bool RenderGeometryFlag = bRenderGeometry;
	bool RenderSpawnPoints = bRenderSpawnPoints;
	bool RenderTangents = bRenderTangents;
	bool RenderTessellationFlag = bRenderTessellation;
	Ar << TangentSpawningScalar << RenderGeometryFlag << RenderSpawnPoints << RenderTangents << RenderTessellationFlag;
	bool EnableTangentDiffInterpScale = bEnableTangentDiffInterpScale;
	Ar << TilingDistance << DistanceTessellationStepSize << EnableTangentDiffInterpScale << TangentTessellationScalar;
	if (Ar.IsLoading())
	{
		bDeadTrailsOnDeactivate = DeadTrailsOnDeactivate;
		bDeadTrailsOnSourceLoss = DeadTrailsOnSourceLoss;
		bClipSourceSegement = ClipSourceSegement;
		bEnablePreviousTangentRecalculation = EnablePreviousTangentRecalculation;
		bTangentRecalculationEveryFrame = TangentRecalculationEveryFrame;
		bSpawnInitialParticle = SpawnInitialParticle;
		bRenderGeometry = RenderGeometryFlag;
		bRenderSpawnPoints = RenderSpawnPoints;
		bRenderTangents = RenderTangents;
		bRenderTessellation = RenderTessellationFlag;
		bEnableTangentDiffInterpScale = EnableTangentDiffInterpScale;
	}
}
