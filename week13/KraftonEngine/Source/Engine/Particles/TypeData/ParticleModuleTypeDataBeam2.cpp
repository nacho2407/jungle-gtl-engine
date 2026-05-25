#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"

#include "Particles/Beam/ParticleModuleBeamModifier.h"
#include "Particles/Beam/ParticleModuleBeamNoise.h"
#include "Particles/Beam/ParticleModuleBeamSource.h"
#include "Particles/Beam/ParticleModuleBeamTarget.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

UParticleModuleTypeDataBeam2::UParticleModuleTypeDataBeam2()
	: bAlwaysOn(false)
	, RenderGeometry(true)
	, RenderDirectLine(false)
	, RenderLines(false)
	, RenderTessellation(false)
{
	InitializeDefaults();
}

void UParticleModuleTypeDataBeam2::InitializeDefaults()
{
	bSpawnModule = true;
	bUpdateModule = true;
	MaxBeamCount = std::max(1, MaxBeamCount);
	Sheets = std::max(1, Sheets);
}

uint32 UParticleModuleTypeDataBeam2::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	uint32 Size = sizeof(FBeam2TypeDataPayload);
	Size += sizeof(FVector) * std::max(0, InterpolationPoints);
	Size += sizeof(float); // NoiseRate
	Size += sizeof(float); // NoiseDeltaTime
	Size += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	Size += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	Size += sizeof(float) * (std::max(0, InterpolationPoints) + 2);
	Size += sizeof(float); // NoiseDistanceScale

	// UE allocates modifier payload through modifier modules, but TypeData owns the shared
	// pointer walk that every beam module uses.
	return Size;
}

void UParticleModuleTypeDataBeam2::GetDataPointers(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, FBeam2TypeDataPayload*& BeamData, FVector*& InterpolatedPoints,
	float*& NoiseRate, float*& NoiseDeltaTime, FVector*& TargetNoisePoints,
	FVector*& NextNoisePoints, float*& TaperValues, float*& NoiseDistanceScale,
	FBeamParticleModifierPayloadData*& SourceModifier,
	FBeamParticleModifierPayloadData*& TargetModifier)
{
	uint8* Base = const_cast<uint8*>(ParticleBase);
	BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FBeam2TypeDataPayload);
	InterpolatedPoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FVector) * std::max(0, InterpolationPoints);
	NoiseRate = reinterpret_cast<float*>(Base + CurrentOffset);
	CurrentOffset += sizeof(float);
	NoiseDeltaTime = reinterpret_cast<float*>(Base + CurrentOffset);
	CurrentOffset += sizeof(float);
	TargetNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	NextNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	TaperValues = reinterpret_cast<float*>(Base + CurrentOffset);
	CurrentOffset += sizeof(float) * (std::max(0, InterpolationPoints) + 2);
	NoiseDistanceScale = reinterpret_cast<float*>(Base + CurrentOffset);
	CurrentOffset += sizeof(float);
	SourceModifier = nullptr;
	TargetModifier = nullptr;
}

void UParticleModuleTypeDataBeam2::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, int32& BeamDataOffset, int32& InterpolatedPointsOffset, int32& NoiseRateOffset,
	int32& NoiseDeltaTimeOffset, int32& TargetNoisePointsOffset, int32& NextNoisePointsOffset,
	int32& TaperCount, int32& TaperValuesOffset, int32& NoiseDistanceScaleOffset)
{
	BeamDataOffset = CurrentOffset;
	CurrentOffset += sizeof(FBeam2TypeDataPayload);
	InterpolatedPointsOffset = CurrentOffset;
	CurrentOffset += sizeof(FVector) * std::max(0, InterpolationPoints);
	NoiseRateOffset = CurrentOffset;
	CurrentOffset += sizeof(float);
	NoiseDeltaTimeOffset = CurrentOffset;
	CurrentOffset += sizeof(float);
	TargetNoisePointsOffset = CurrentOffset;
	CurrentOffset += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	NextNoisePointsOffset = CurrentOffset;
	CurrentOffset += sizeof(FVector) * FDynamicBeam2EmitterData::MaxNoiseFrequency;
	TaperCount = std::max(0, InterpolationPoints) + 2;
	TaperValuesOffset = CurrentOffset;
	CurrentOffset += sizeof(float) * TaperCount;
	NoiseDistanceScaleOffset = CurrentOffset;
	CurrentOffset += sizeof(float);
}

void UParticleModuleTypeDataBeam2::Spawn(const FSpawnContext& Context)
{
	int32 CurrentOffset = Context.Owner.TypeDataOffset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FVector* InterpolatedPoints = nullptr;
	float* NoiseRate = nullptr;
	float* NoiseDeltaTime = nullptr;
	FVector* TargetNoisePoints = nullptr;
	FVector* NextNoisePoints = nullptr;
	float* TaperValues = nullptr;
	float* NoiseDistanceScale = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	GetDataPointers(&Context.Owner, reinterpret_cast<const uint8*>(Context.ParticleBase), CurrentOffset, BeamData, InterpolatedPoints,
		NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints, TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	if (!BeamData)
	{
		return;
	}

	const FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<const FParticleBeam2EmitterInstance*>(&Context.Owner);
	const bool bHasSourceModule = BeamInst && BeamInst->BeamModule_Source;
	const bool bHasTargetModule = BeamInst && BeamInst->BeamModule_Target;

	if (!bHasSourceModule)
	{
		BeamData->SourcePoint = Context.Owner.Location;
		BeamData->SourceTangent = FVector::XAxisVector;
		BeamData->SourceStrength = 0.0f;
	}

	if (!bHasTargetModule && BeamMethod == PEB2M_Distance)
	{
		const float BeamDistance = Distance.GetValue(Context.Owner.EmitterTime, Context.GetDistributionData());
		BeamData->TargetPoint = BeamData->SourcePoint + FVector(BeamDistance, 0.0f, 0.0f);
		BeamData->TargetTangent = FVector::XAxisVector;
		BeamData->TargetStrength = 0.0f;
	}

	if (SourceModifier)
	{
		SourceModifier->UpdatePosition(BeamData->SourcePoint);
		SourceModifier->UpdateTangent(BeamData->SourceTangent, false);
		SourceModifier->UpdateStrength(BeamData->SourceStrength);
	}
	if (TargetModifier)
	{
		TargetModifier->UpdatePosition(BeamData->TargetPoint);
		TargetModifier->UpdateTangent(BeamData->TargetTangent, false);
		TargetModifier->UpdateStrength(BeamData->TargetStrength);
	}

	BeamData->Lock_Max_NumNoisePoints = 0;
	BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, Speed <= 0.0f);
	Context.ParticleBase->Location = Speed > 0.0f ? BeamData->SourcePoint : BeamData->TargetPoint;

	Update({ Context.Owner, Context.Owner.TypeDataOffset, 0.0f });
}

void UParticleModuleTypeDataBeam2::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP;
	int32 LocalOffset = Context.Owner.TypeDataOffset;
	FBeam2TypeDataPayload* BeamData = nullptr;
	FVector* InterpolatedPoints = nullptr;
	float* NoiseRate = nullptr;
	float* NoiseDeltaTime = nullptr;
	FVector* TargetNoisePoints = nullptr;
	FVector* NextNoisePoints = nullptr;
	float* TaperValues = nullptr;
	float* NoiseDistanceScale = nullptr;
	FBeamParticleModifierPayloadData* SourceModifier = nullptr;
	FBeamParticleModifierPayloadData* TargetModifier = nullptr;
	GetDataPointers(&Context.Owner, ParticleBase, LocalOffset, BeamData, InterpolatedPoints,
		NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints, TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	const FVector FullDelta = BeamData->TargetPoint - BeamData->SourcePoint;
	const float FullDistance = FullDelta.Length();
	BeamData->Direction = FullDelta.GetSafeNormal(1.0e-6f, FVector::XAxisVector);

	if (Speed > 0.0f && !BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
	{
		const FVector CurrentDelta = Particle.Location - BeamData->SourcePoint;
		const float CurrentDistance = CurrentDelta.Length();
		float NewDistance = CurrentDistance + Speed * Context.DeltaTime;
		if (NewDistance >= FullDistance)
		{
			NewDistance = FullDistance;
			BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, true);
		}
		Particle.Location = BeamData->SourcePoint + BeamData->Direction * NewDistance;
	}
	else
	{
		Particle.Location = BeamData->TargetPoint;
		BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, true);
	}

	const float RenderDistance = FVector::Distance(BeamData->SourcePoint, Particle.Location);
	BeamData->TravelRatio = FullDistance > 1.0e-6f ? std::min(RenderDistance / FullDistance, 1.0f) : 1.0f;
	BeamData->InterpolationSteps = std::max(0, InterpolationPoints);
	BeamData->Steps = std::max(1, BeamData->InterpolationSteps + 1);
	BeamData->StepSize = RenderDistance / static_cast<double>(BeamData->Steps);
	BeamData->TriangleCount = BeamData->Steps * std::max(1, Sheets) * 2;
	if (TaperValues)
	{
		const int32 TaperCount = BeamData->Steps + 1;
		for (int32 TaperIndex = 0; TaperIndex < TaperCount; ++TaperIndex)
		{
			const float Alpha = TaperCount > 1 ? static_cast<float>(TaperIndex) / static_cast<float>(TaperCount - 1) : 0.0f;
			const float Factor = TaperFactor.GetValue(Alpha, Context.GetDistributionData());
			const float Scale = TaperScale.GetValue(Alpha, Context.GetDistributionData());
			TaperValues[TaperIndex] = (TaperMethod == PEBTM_None) ? 1.0f : Factor * Scale;
		}
	}
	END_UPDATE_LOOP;
}

FParticleEmitterInstance* UParticleModuleTypeDataBeam2::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent)
{
	return new FParticleBeam2EmitterInstance();
}

void UParticleModuleTypeDataBeam2::CacheModuleInfo(UParticleEmitter* Emitter)
{
	LOD_BeamModule_Source.clear();
	LOD_BeamModule_Target.clear();
	LOD_BeamModule_Noise.clear();
	LOD_BeamModule_SourceModifier.clear();
	LOD_BeamModule_TargetModifier.clear();

	if (!Emitter)
	{
		return;
	}

	for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
	{
		UParticleModuleBeamSource* SourceModule = nullptr;
		UParticleModuleBeamTarget* TargetModule = nullptr;
		UParticleModuleBeamNoise* NoiseModule = nullptr;
		UParticleModuleBeamModifier* SourceModifier = nullptr;
		UParticleModuleBeamModifier* TargetModifier = nullptr;
		if (LODLevel)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				if (auto* Source = Cast<UParticleModuleBeamSource>(Module)) SourceModule = Source;
				if (auto* Target = Cast<UParticleModuleBeamTarget>(Module)) TargetModule = Target;
				if (auto* Noise = Cast<UParticleModuleBeamNoise>(Module)) NoiseModule = Noise;
				if (auto* Modifier = Cast<UParticleModuleBeamModifier>(Module))
				{
					if (Modifier->ModifierType == PEB2MT_Source) SourceModifier = Modifier;
					else if (Modifier->ModifierType == PEB2MT_Target) TargetModifier = Modifier;
				}
			}
		}
		LOD_BeamModule_Source.push_back(SourceModule);
		LOD_BeamModule_Target.push_back(TargetModule);
		LOD_BeamModule_Noise.push_back(NoiseModule);
		LOD_BeamModule_SourceModifier.push_back(SourceModifier);
		LOD_BeamModule_TargetModifier.push_back(TargetModifier);
	}
}

void UParticleModuleTypeDataBeam2::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
	NoiseMin = FVector::ZeroVector;
	NoiseMax = FVector::ZeroVector;
}

void UParticleModuleTypeDataBeam2::Serialize(FArchive& Ar)
{
	UParticleModuleTypeDataBase::Serialize(Ar);
	Ar << reinterpret_cast<int32&>(BeamMethod);
	bool AlwaysOn = bAlwaysOn;
	Ar << TextureTile << TextureTileDistance << Sheets << MaxBeamCount << Speed << InterpolationPoints << AlwaysOn << UpVectorStepSize;
	Ar << BranchParentName;
	Distance.Serialize(Ar);
	Ar << reinterpret_cast<int32&>(TaperMethod);
	TaperFactor.Serialize(Ar);
	TaperScale.Serialize(Ar);
	bool RenderGeometryFlag = RenderGeometry;
	bool RenderDirectLineFlag = RenderDirectLine;
	bool RenderLinesFlag = RenderLines;
	bool RenderTessellationFlag = RenderTessellation;
	Ar << RenderGeometryFlag << RenderDirectLineFlag << RenderLinesFlag << RenderTessellationFlag;
	if (Ar.IsLoading())
	{
		bAlwaysOn = AlwaysOn;
		RenderGeometry = RenderGeometryFlag;
		RenderDirectLine = RenderDirectLineFlag;
		RenderLines = RenderLinesFlag;
		RenderTessellation = RenderTessellationFlag;
	}
}
