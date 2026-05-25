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

UParticleModuleBeamNoise* GetFirstBeamNoiseModuleForLayout(UParticleModuleTypeDataBeam2* TypeData)
{
	return (TypeData && TypeData->LOD_BeamModule_Noise.size() > 0) ? TypeData->LOD_BeamModule_Noise[0] : nullptr;
}

uint32 UParticleModuleTypeDataBeam2::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	uint32 Size = 0;
	int32 TaperCount = 2;

	Size += sizeof(FBeam2TypeDataPayload);

	if (InterpolationPoints > 0)
	{
		Size += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	UParticleModuleBeamNoise* BeamNoise = GetFirstBeamNoiseModuleForLayout(this);
	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		Size += sizeof(float);              // NoiseRate
		Size += sizeof(float);              // NoiseDeltaTime
		Size += sizeof(FVector) * Frequency; // TargetNoisePoints

		if (BeamNoise->bSmooth)
		{
			Size += sizeof(FVector) * Frequency; // NextNoisePoints
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			Size += sizeof(float); // NoiseDistanceScale
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		Size += sizeof(float) * TaperCount;
	}

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
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	UParticleModuleBeamNoise* BeamNoise = BeamInst ? BeamInst->BeamModule_Noise : nullptr;
	int32 TaperCount = 2;

	BeamData = reinterpret_cast<FBeam2TypeDataPayload*>(Base + CurrentOffset);
	CurrentOffset += sizeof(FBeam2TypeDataPayload);

	InterpolatedPoints = nullptr;
	NoiseRate = nullptr;
	NoiseDeltaTime = nullptr;
	TargetNoisePoints = nullptr;
	NextNoisePoints = nullptr;
	TaperValues = nullptr;
	NoiseDistanceScale = nullptr;

	if (InterpolationPoints > 0)
	{
		InterpolatedPoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
		CurrentOffset += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		NoiseRate = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float);
		NoiseDeltaTime = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float);

		TargetNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
		CurrentOffset += sizeof(FVector) * Frequency;

		if (BeamNoise->bSmooth)
		{
			NextNoisePoints = reinterpret_cast<FVector*>(Base + CurrentOffset);
			CurrentOffset += sizeof(FVector) * Frequency;
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			NoiseDistanceScale = reinterpret_cast<float*>(Base + CurrentOffset);
			CurrentOffset += sizeof(float);
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		TaperValues = reinterpret_cast<float*>(Base + CurrentOffset);
		CurrentOffset += sizeof(float) * TaperCount;
	}

	SourceModifier = (BeamInst && BeamInst->BeamModule_SourceModifier_Offset != INDEX_NONE)
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(Base + BeamInst->BeamModule_SourceModifier_Offset)
		: nullptr;
	TargetModifier = (BeamInst && BeamInst->BeamModule_TargetModifier_Offset != INDEX_NONE)
		? reinterpret_cast<FBeamParticleModifierPayloadData*>(Base + BeamInst->BeamModule_TargetModifier_Offset)
		: nullptr;
}

void UParticleModuleTypeDataBeam2::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const uint8* ParticleBase,
	int32& CurrentOffset, int32& BeamDataOffset, int32& InterpolatedPointsOffset, int32& NoiseRateOffset,
	int32& NoiseDeltaTimeOffset, int32& TargetNoisePointsOffset, int32& NextNoisePointsOffset,
	int32& TaperCount, int32& TaperValuesOffset, int32& NoiseDistanceScaleOffset)
{
	FParticleBeam2EmitterInstance* BeamInst = dynamic_cast<FParticleBeam2EmitterInstance*>(Owner);
	UParticleModuleBeamNoise* BeamNoise = BeamInst ? BeamInst->BeamModule_Noise : nullptr;
	int32 LocalOffset = 0;

	BeamDataOffset = CurrentOffset + LocalOffset;
	LocalOffset += sizeof(FBeam2TypeDataPayload);

	InterpolatedPointsOffset = INDEX_NONE;
	NoiseRateOffset = INDEX_NONE;
	NoiseDeltaTimeOffset = INDEX_NONE;
	TargetNoisePointsOffset = INDEX_NONE;
	NextNoisePointsOffset = INDEX_NONE;
	TaperValuesOffset = INDEX_NONE;
	NoiseDistanceScaleOffset = INDEX_NONE;
	TaperCount = 2;

	if (InterpolationPoints > 0)
	{
		InterpolatedPointsOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(FVector) * InterpolationPoints;
		TaperCount = InterpolationPoints + 1;
	}

	if (BeamNoise && BeamNoise->bLowFreq_Enabled)
	{
		const int32 Frequency = BeamNoise->Frequency + 1;

		NoiseRateOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float);
		NoiseDeltaTimeOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float);

		TargetNoisePointsOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(FVector) * Frequency;

		if (BeamNoise->bSmooth)
		{
			NextNoisePointsOffset = CurrentOffset + LocalOffset;
			LocalOffset += sizeof(FVector) * Frequency;
		}

		TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

		if (BeamNoise->bApplyNoiseScale)
		{
			NoiseDistanceScaleOffset = CurrentOffset + LocalOffset;
			LocalOffset += sizeof(float);
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		TaperValuesOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(float) * TaperCount;
	}

	CurrentOffset += LocalOffset;
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

	BeamData->Lock_Max_NumNoisePoints = 0;
	BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, Speed <= 0.0f);
	Context.ParticleBase->Location = Speed > 0.0f ? BeamData->SourcePoint : BeamData->TargetPoint;
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

	if (!BeamData)
	{
		continue;
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
		int32 TaperCount = BeamData->Steps + 1;
		if (Context.Owner.CurrentLODLevel && Context.Owner.CurrentLODLevel->TypeDataModule == this)
		{
			int32 DummyOffset = Context.Owner.TypeDataOffset;
			int32 DummyBeamOffset, DummyInterpOffset, DummyNoiseRate, DummyNoiseDelta, DummyTargetNoise, DummyNextNoise, DummyTaperOffset, DummyNoiseScale;
			GetDataPointerOffsets(&Context.Owner, ParticleBase, DummyOffset, DummyBeamOffset, DummyInterpOffset, DummyNoiseRate, DummyNoiseDelta, DummyTargetNoise, DummyNextNoise, TaperCount, DummyTaperOffset, DummyNoiseScale);
		}

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
				const bool bIsSource = Cast<UParticleModuleBeamSource>(Module) != nullptr;
				const bool bIsTarget = Cast<UParticleModuleBeamTarget>(Module) != nullptr;
				const bool bIsNoise = Cast<UParticleModuleBeamNoise>(Module) != nullptr;
				const bool bIsModifier = Cast<UParticleModuleBeamModifier>(Module) != nullptr;
				const bool bBeamModule = bIsSource || bIsTarget || bIsNoise || bIsModifier;

				if (bIsSource) SourceModule = Cast<UParticleModuleBeamSource>(Module);
				if (bIsTarget) TargetModule = Cast<UParticleModuleBeamTarget>(Module);
				if (bIsNoise) NoiseModule = Cast<UParticleModuleBeamNoise>(Module);
				if (auto* Modifier = Cast<UParticleModuleBeamModifier>(Module))
				{
					if (Modifier->ModifierType == PEB2MT_Source) SourceModifier = Modifier;
					else if (Modifier->ModifierType == PEB2MT_Target) TargetModifier = Modifier;
				}

				if (bBeamModule)
				{
					LODLevel->SpawnModules.erase(
						std::remove(LODLevel->SpawnModules.begin(), LODLevel->SpawnModules.end(), Module),
						LODLevel->SpawnModules.end());
					LODLevel->UpdateModules.erase(
						std::remove(LODLevel->UpdateModules.begin(), LODLevel->UpdateModules.end(), Module),
						LODLevel->UpdateModules.end());
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
