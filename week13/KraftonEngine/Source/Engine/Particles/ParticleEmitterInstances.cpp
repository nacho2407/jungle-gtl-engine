#include "Particles/ParticleEmitterInstances.h"

#include "Particles/ParticleMemory.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleEmitter.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Spawn/ParticleModuleSpawnPerUnit.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Particles/Beam/ParticleModuleBeamSource.h"
#include "Particles/Beam/ParticleModuleBeamTarget.h"
#include "Particles/Beam/ParticleModuleBeamNoise.h"
#include "Particles/Beam/ParticleModuleBeamModifier.h"
#include "Particles/Trail/ParticleModuleTrailSource.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Material/ParticleModuleMeshMaterial.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Math/Rotator.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
	FQuat FindBetweenNormals(const FVector& From, const FVector& To)
	{
		const FVector A = From.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		const FVector B = To.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		const float CosTheta = FMath::Clamp(A.Dot(B), -1.0f, 1.0f);

		if (CosTheta > 1.0f - 1.0e-6f)
		{
			return FQuat::Identity;
		}

		if (CosTheta < -1.0f + 1.0e-6f)
		{
			FVector Axis = FVector::ZAxisVector.Cross(A);
			if (Axis.IsNearlyZero())
			{
				Axis = FVector::YAxisVector.Cross(A);
			}
			Axis.Normalize();
			return FQuat::FromAxisAngle(Axis, FMath::Pi);
		}

		const FVector Axis = A.Cross(B);
		FQuat Result(Axis.X, Axis.Y, Axis.Z, 1.0f + CosTheta);
		Result.Normalize();
		return Result;
	}

	FVector GetAxisLockVector(EParticleAxisLock AxisLock)
	{
		switch (AxisLock)
		{
		case EPAL_X: return FVector::XAxisVector;
		case EPAL_Y: return FVector::YAxisVector;
		case EPAL_NEGATIVE_X: return -FVector::XAxisVector;
		case EPAL_NEGATIVE_Y: return -FVector::YAxisVector;
		case EPAL_NEGATIVE_Z: return -FVector::ZAxisVector;
		case EPAL_Z:
		case EPAL_NONE:
		default:
			return FVector::ZAxisVector;
		}
	}

	FVector CubicInterpDerivativeVector(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float Alpha)
	{
		const float A2 = Alpha * Alpha;
		return P0 * (6.0f * A2 - 6.0f * Alpha)
			+ T0 * (3.0f * A2 - 4.0f * Alpha + 1.0f)
			+ P1 * (-6.0f * A2 + 6.0f * Alpha)
			+ T1 * (3.0f * A2 - 2.0f * Alpha);
	}

	FVector CubicInterpVector(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float Alpha)
	{
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;
		return P0 * (2.0f * A3 - 3.0f * A2 + 1.0f)
			+ T0 * (A3 - 2.0f * A2 + Alpha)
			+ P1 * (-2.0f * A3 + 3.0f * A2)
			+ T1 * (A3 - A2);
	}

	int32 ClampParticleCountToUInt16(int32 Count)
	{
		return std::min<int32>(Count, std::numeric_limits<uint16>::max());
	}

	bool IsReplayType(const FDynamicEmitterReplayDataBase& Data, EDynamicEmitterType ExpectedType)
	{
		return Data.eEmitterType == ExpectedType;
	}

	void TrailsBase_CalculateTangent(
		FBaseParticle* InPrevParticle,
		FRibbonTypeDataPayload* InPrevTrailData,
		FBaseParticle* InNextParticle,
		FRibbonTypeDataPayload* InNextTrailData,
		float InCurrNextDelta,
		FRibbonTypeDataPayload* InOutCurrTrailData)
	{
		if (!InPrevParticle || !InPrevTrailData || !InNextParticle || !InNextTrailData || !InOutCurrTrailData)
		{
			return;
		}

		FVector PositionDelta = InPrevParticle->Location - InNextParticle->Location;
		float TimeDelta = InPrevTrailData->SpawnTime - InNextTrailData->SpawnTime;
		TimeDelta = (TimeDelta == 0.0f) ? 0.0032f : std::fabs(TimeDelta);

		FVector NewTangent = PositionDelta / TimeDelta;
		NewTangent *= InCurrNextDelta;
		NewTangent *= (1.0f / std::max(1, InOutCurrTrailData->SpawnedTessellationPoints));
		InOutCurrTrailData->Tangent = NewTangent;
	}

	void CopyActiveParticlesToReplay(
		const FParticleEmitterInstance& Instance,
		FDynamicEmitterReplayDataBase& OutData)
	{
		assert(Instance.ParticleData != nullptr);
		assert(Instance.ParticleIndices != nullptr);
		assert(Instance.MaxActiveParticles >= Instance.ActiveParticles);

		OutData.ActiveParticleCount = Instance.ActiveParticles;
		OutData.ParticleStride = Instance.ParticleStride;
		OutData.SortMode = static_cast<EParticleSortMode>(Instance.SortMode);
		OutData.Scale = Instance.Component->GetWorldScale();
		OutData.MaxDrawCount = -1;
		UParticleModuleRequired* RequiredModule = Instance.GetCurrentLODLevelChecked()->RequiredModule;
		if (RequiredModule->bUseMaxDrawCount)
		{
			OutData.MaxDrawCount = RequiredModule->MaxDrawCount;
		}

		const int32 ParticleDataBytes =
			Instance.ParticleStride * Instance.MaxActiveParticles;

		OutData.DataContainer.Alloc(
			ParticleDataBytes,
			Instance.MaxActiveParticles);

		std::memcpy(OutData.DataContainer.ParticleData, Instance.ParticleData, ParticleDataBytes);
		std::memcpy(
			OutData.DataContainer.ParticleIndices,
			Instance.ParticleIndices,
			static_cast<size_t>(Instance.MaxActiveParticles) * sizeof(uint16));
	}
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	FreeResources();
}

void FParticleEmitterInstance::InitParameters(
	UParticleEmitter* InTemplate,
	UParticleSystemComponent* InComponent)
{
	assert(InTemplate != nullptr);
	assert(InComponent != nullptr);

	SpriteTemplate = InTemplate;
	Component = InComponent;

	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate->GetLODLevel(0);
	assert(CurrentLODLevel != nullptr);
	assert(CurrentLODLevel->RequiredModule != nullptr);

	TypeDataOffset = SpriteTemplate->TypeDataOffset;
	TypeDataInstanceOffset = SpriteTemplate->TypeDataInstanceOffset;

	DynamicParameterDataOffset = SpriteTemplate->DynamicParameterDataOffset;
	LightDataOffset = SpriteTemplate->LightDataOffset;
	OrbitModuleOffset = SpriteTemplate->OrbitModuleOffset;
	CameraPayloadOffset = SpriteTemplate->CameraPayloadOffset;

	ParticleSize = SpriteTemplate->ParticleSize;
	InstancePayloadSize = SpriteTemplate->ReqInstanceBytes;
	PivotOffset = SpriteTemplate->PivotOffset;

	SortMode = CurrentLODLevel->RequiredModule->SortMode;

	SetupEmitterDuration();
}

void FParticleEmitterInstance::Init()
{
	assert(SpriteTemplate != nullptr);

	UParticleLODLevel* HighLODLevel = SpriteTemplate->GetLODLevel(0);
	assert(HighLODLevel != nullptr);
	assert(HighLODLevel->RequiredModule != nullptr);

	assert(CurrentLODLevel != nullptr);

	HighLODLevel->RequiredModule->ResolveMaterialFromSlot();
	CurrentMaterial = HighLODLevel->RequiredModule->Material;
	bKillOnDeactivate = HighLODLevel->RequiredModule->bKillOnDeactivate;
	bKillOnCompleted = HighLODLevel->RequiredModule->bKillOnCompleted;
	SortMode = HighLODLevel->RequiredModule->SortMode;

	ActiveParticles = 0;
	MaxActiveParticles = 0;
	PeakActiveParticles = 0;

	SpawnFraction = 0.0f;
	SecondsSinceCreation = 0.0f;
	EmitterTime = 0.0f;
	LastDeltaTime = 0.0f;

	ParticleCounter = 0;
	LoopCount = 0;

	bEmitterIsDone = false;
	bHaltSpawning = false;
	bHaltSpawningExternal = false;

	ParticleSize = SpriteTemplate->ParticleSize;
	InstancePayloadSize = SpriteTemplate->ReqInstanceBytes;
	PayloadOffset = ParticleSize;

	ParticleSize += static_cast<int32>(RequiredBytes());
	ParticleSize = static_cast<int32>(
		ParticleMemory::AlignSize(static_cast<size_t>(ParticleSize)));

	ParticleStride =
		static_cast<int32>(CalculateParticleStride(static_cast<uint32>(ParticleSize)));

	assert((ParticleStride % 16) == 0);

	if (InstancePayloadSize > 0)
	{
		InstanceData = static_cast<uint8*>(
			ParticleMemory::Malloc(static_cast<size_t>(InstancePayloadSize)));
		std::memset(InstanceData, 0, static_cast<size_t>(InstancePayloadSize));
	}

	BurstFired.clear();

	BurstFired.resize(SpriteTemplate->LODLevels.size());

	for (int32 LODIndex = 0;
		LODIndex < static_cast<int32>(SpriteTemplate->LODLevels.size());
		++LODIndex)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(LODIndex);
		assert(LODLevel != nullptr);

		if (LODLevel->SpawnModule)
		{
			BurstFired[LODIndex].Fired.resize(
				LODLevel->SpawnModule->BurstList.size(), false);
		}
	}

	SetupEmitterDuration();
	ResetBurstList();

	UpdateTransforms();

	Location = Component->GetWorldLocation();
	OldLocation = Location;

	ParticleBoundingBox = FBoundingBox();
	TrianglesToRender = 0;
	MaxVertexIndex = 0;

	if (Component->IsGameWorld() && SpriteTemplate->QualityLevelSpawnRateScale > 0.0f)
	{
		const int32 InitialCount =
			SpriteTemplate->InitialAllocationCount > 0
			? std::min(SpriteTemplate->InitialAllocationCount, 100)
			: (HighLODLevel->PeakActiveParticles > 0
				? std::min(HighLODLevel->PeakActiveParticles, 100)
				: 10);

		Resize(InitialCount, false);
	}

	IsRenderDataDirty = 1;
}

void FParticleEmitterInstance::FreeResources()
{
	if (ParticleData)
	{
		const size_t ParticleBytes =
			static_cast<size_t>(ParticleStride) * static_cast<size_t>(MaxActiveParticles);
		ParticleMemory::Free(ParticleData, ParticleBytes);
		ParticleData = nullptr;
	}

	if (ParticleIndices)
	{
		const size_t IndexBytes =
			sizeof(uint16) * static_cast<size_t>(MaxActiveParticles + 1);
		ParticleMemory::Free(ParticleIndices, IndexBytes);
		ParticleIndices = nullptr;
	}

	if (InstanceData)
	{
		ParticleMemory::Free(InstanceData, static_cast<size_t>(InstancePayloadSize));
		InstanceData = nullptr;
	}

	ActiveParticles = 0;
	MaxActiveParticles = 0;
	PeakActiveParticles = 0;
}

bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	if (NewMaxActiveParticles <= MaxActiveParticles)
	{
		return true;
	}

	assert(ParticleStride > 0);
	assert((ParticleStride % 16) == 0);

	NewMaxActiveParticles = ClampParticleCountToUInt16(NewMaxActiveParticles);

	if (NewMaxActiveParticles <= MaxActiveParticles)
	{
		return true;
	}

	const int32 OldMaxActiveParticles = MaxActiveParticles;

	const size_t OldParticleBytes =
		static_cast<size_t>(ParticleStride) * static_cast<size_t>(OldMaxActiveParticles);
	const size_t NewParticleBytes =
		static_cast<size_t>(ParticleStride) * static_cast<size_t>(NewMaxActiveParticles);

	ParticleData = static_cast<uint8*>(
		ParticleMemory::Realloc(ParticleData, OldParticleBytes, NewParticleBytes));

	assert(ParticleData != nullptr);
	assert((reinterpret_cast<uintptr_t>(ParticleData) % 16) == 0);

	const size_t OldIndexBytes =
		sizeof(uint16) * static_cast<size_t>(OldMaxActiveParticles + 1);
	const size_t NewIndexBytes =
		sizeof(uint16) * static_cast<size_t>(NewMaxActiveParticles + 1);

	ParticleIndices = static_cast<uint16*>(
		ParticleMemory::Realloc(ParticleIndices, OldIndexBytes, NewIndexBytes));

	assert(ParticleIndices != nullptr);

	for (int32 i = OldMaxActiveParticles; i < NewMaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
	}

	ParticleIndices[NewMaxActiveParticles] =
		static_cast<uint16>(NewMaxActiveParticles - 1);

	MaxActiveParticles = NewMaxActiveParticles;

	if (bSetMaxActiveCount && NewMaxActiveParticles > PeakActiveParticles)
	{
		PeakActiveParticles = NewMaxActiveParticles;

		if (SpriteTemplate)
		{
			UParticleLODLevel* HighLODLevel = SpriteTemplate->GetLODLevel(0);
			if (HighLODLevel && NewMaxActiveParticles > HighLODLevel->PeakActiveParticles)
			{
				HighLODLevel->PeakActiveParticles = NewMaxActiveParticles;
			}
		}
	}

	return true;
}

void FParticleEmitterInstance::UpdateTransforms()
{
	assert(SpriteTemplate != nullptr);
	assert(Component != nullptr);

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel != nullptr);
	assert(LODLevel->RequiredModule != nullptr);

	const FMatrix ComponentWorld = Component->GetWorldMatrix();

	FMatrix ComponentToWorldNoScale = ComponentWorld;
	ComponentToWorldNoScale.RemoveScaling();

	FMatrix EmitterRotation =
		LODLevel->RequiredModule->EmitterRotation.ToMatrix();

	EmitterRotation.SetLocation(LODLevel->RequiredModule->EmitterOrigin);

	const FMatrix EmitterToComponent = EmitterRotation;

	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		EmitterToSimulation = EmitterToComponent;
		SimulationToWorld = ComponentToWorldNoScale;

		if (SimulationToWorld.ContainsNaN())
		{
			SimulationToWorld = FMatrix::Identity;
		}
	}
	else
	{
		EmitterToSimulation = EmitterToComponent * ComponentToWorldNoScale;
		SimulationToWorld = FMatrix::Identity;
	}
}

void FParticleEmitterInstance::ApplyWorldOffset(FVector InOffset, bool bWorldShift)
{
	UpdateTransforms();

	Location += InOffset;
	OldLocation += InOffset;

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (!LODLevel->RequiredModule->bUseLocalSpace)
	{
		PositionOffsetThisTick = InOffset;
	}
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	SCOPE_STAT_CAT("ParticleEmitterInstance_Tick", "Particles");

	if (bEmitterIsDone)
	{
		return;
	}

	const bool bFirstTime = SecondsSinceCreation <= 0.0f;
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	// Cascade 원본처럼 이 함수는 "effective delta"가 아니라 emitter delay를 반환한다.
	const float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

	if (bEnabled)
	{
		KillParticles();

		if (bUseParticlePrefetch)
			ParticlePrefetch();

		ResetParticleParameters(DeltaTime);

		LODLevel->RequiredModule->ResolveMaterialFromSlot();
		CurrentMaterial = LODLevel->RequiredModule->Material;
		Tick_ModuleUpdate(DeltaTime, LODLevel);

		SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSuppressSpawning, bFirstTime);

		Tick_ModulePostUpdate(DeltaTime, LODLevel);

		if (ActiveParticles > 0)
		{
			if (bUseParticlePrefetch)
				ParticlePrefetch();

			UpdateOrbitData(DeltaTime);
			UpdateBoundingBox(DeltaTime);
		}

		Tick_ModuleFinalUpdate(DeltaTime, LODLevel);
		CheckEmitterFinished();
		IsRenderDataDirty = 1;
	}
	else
	{
		FakeBursts();
	}

	// Tick_EmitterTimeSetup()에서 module 평가용으로 delay만큼 빼뒀기 때문에 tick 끝에서 되돌린다.
	EmitterTime += EmitterDelay;
	LastDeltaTime = DeltaTime;
	PositionOffsetThisTick = FVector::ZeroVector;
}

void FParticleEmitterInstance::CheckEmitterFinished()
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel->RequiredModule != nullptr);
	assert(LODLevel->SpawnModule != nullptr);

	if (ActiveParticles != 0)
	{
		return;
	}

	const FParticleBurst* LastBurst = nullptr;
	if (!LODLevel->SpawnModule->BurstList.empty())
	{
		LastBurst = &LODLevel->SpawnModule->BurstList.back();
	}

	if (!LastBurst || LastBurst->Time < EmitterTime)
	{
		const bool bNoContinuousSpawning = LODLevel->SpawnModule->SpawnRate <= 0.0f;
		const bool bInfiniteZeroDuration =
			bNoContinuousSpawning &&
			LODLevel->RequiredModule->EmitterDuration <= 0.0f &&
			LODLevel->RequiredModule->EmitterLoops == 0;

		if (HasCompleted() || bInfiniteZeroDuration)
		{
			bEmitterIsDone = true;
		}
	}
}


float FParticleEmitterInstance::Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	OldLocation = Location;
	Location = Component->GetWorldLocation();

	UpdateTransforms();
	SecondsSinceCreation += DeltaTime;

	assert(InCurrentLODLevel != nullptr);
	assert(InCurrentLODLevel->RequiredModule != nullptr);

	UParticleModuleRequired* RequiredModule = InCurrentLODLevel->RequiredModule;

	bool bLooped = false;
	EmitterTime += DeltaTime;
	bLooped = (EmitterDuration > 0.0f) && (EmitterTime >= EmitterDuration);

	float EmitterDelay = CurrentDelay;

	if (bLooped)
	{
		LoopCount++;
		ResetBurstList();

		if (EventCount > MaxEventCount)
		{
			MaxEventCount = EventCount;
		}

		EventCount = 0;

		EmitterTime -= EmitterDuration;

		//if (RequiredModule->bDurationRecalcEachLoop ||
		//	(RequiredModule->bDelayFirstLoopOnly && LoopCount == 1))
		//{
		//	SetupEmitterDuration();
		//}
	}

	//if (RequiredModule->bDelayFirstLoopOnly && LoopCount > 0)
	//{
	//	EmitterDelay = 0.0f;
	//}

	EmitterTime -= EmitterDelay;
	return EmitterDelay;
}


float FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime)
{
	assert(InCurrentLODLevel != nullptr);
	assert(InCurrentLODLevel->RequiredModule != nullptr);

	if (!bHaltSpawning && !bHaltSpawningExternal && !bSuppressSpawning && (EmitterTime >= 0.0f))
	{
		// If emitter is not done - spawn at current rate.
		// If EmitterLoops is 0, then we loop forever, so always spawn.
		if ((InCurrentLODLevel->RequiredModule->EmitterLoops == 0) ||
			(LoopCount < InCurrentLODLevel->RequiredModule->EmitterLoops) ||
			(SecondsSinceCreation < (EmitterDuration * InCurrentLODLevel->RequiredModule->EmitterLoops)) ||
			bFirstTime)
		{
			bFirstTime = false;
			SpawnFraction = Spawn(DeltaTime);
		}
	}
	else if (bFakeBurstsWhenSpawningSupressed)
	{
		FakeBursts();
	}

	return SpawnFraction;
}

void FParticleEmitterInstance::Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	assert(SpriteTemplate != nullptr);
	assert(InCurrentLODLevel != nullptr);
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
	for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.size(); ModuleIndex++)
	{
		UParticleModule* CurrentModule = InCurrentLODLevel->UpdateModules[ModuleIndex];
		if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule)
		{
			CurrentModule->Update({ *this, (int32)GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime });
		}
	}
}


void FParticleEmitterInstance::Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	assert(InCurrentLODLevel != nullptr);

	// Handle the TypeData module
	if (InCurrentLODLevel->TypeDataModule)
	{
		InCurrentLODLevel->TypeDataModule->Update({ *this, TypeDataOffset, DeltaTime });
	}
}


void FParticleEmitterInstance::Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	assert(SpriteTemplate != nullptr);
	assert(InCurrentLODLevel != nullptr);
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
	for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.size(); ModuleIndex++)
	{
		UParticleModule* CurrentModule = InCurrentLODLevel->UpdateModules[ModuleIndex];
		if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bFinalUpdateModule)
		{
			CurrentModule->FinalUpdate({ *this, (int32)GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime });
		}
	}

	if (InCurrentLODLevel->TypeDataModule && InCurrentLODLevel->TypeDataModule->bEnabled && InCurrentLODLevel->TypeDataModule->bFinalUpdateModule)
	{
		InCurrentLODLevel->TypeDataModule->FinalUpdate({ *this, (int32)GetModuleDataOffset(HighestLODLevel->TypeDataModule), DeltaTime });
	}
}

void FParticleEmitterInstance::SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess)
{
	assert(SpriteTemplate != nullptr);
	assert(Component != nullptr);

	// Unreal처럼 잘못된 LOD index가 들어와도 LOD0으로 fallback한다.
	CurrentLODLevelIndex = InLODIndex;

	if (InLODIndex >= 0 &&
		InLODIndex < static_cast<int32>(SpriteTemplate->LODLevels.size()))
	{
		CurrentLODLevel = SpriteTemplate->LODLevels[InLODIndex];
	}
	else
	{
		CurrentLODLevelIndex = 0;
		CurrentLODLevel =
			!SpriteTemplate->LODLevels.empty()
			? SpriteTemplate->LODLevels[0]
			: nullptr;
	}

	assert(CurrentLODLevel != nullptr);
	assert(CurrentLODLevel->RequiredModule != nullptr);

	if (CurrentLODLevelIndex >= 0 &&
		CurrentLODLevelIndex < static_cast<int32>(EmitterDurations.size()))
	{
		EmitterDuration = EmitterDurations[CurrentLODLevelIndex];
	}

	SortMode = CurrentLODLevel->RequiredModule->SortMode;

	if (bInFullyProcess)
	{
		bKillOnCompleted = CurrentLODLevel->RequiredModule->bKillOnCompleted;
		bKillOnDeactivate = CurrentLODLevel->RequiredModule->bKillOnDeactivate;

		UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;
		assert(SpawnModule != nullptr);

		if (CurrentLODLevelIndex + 1 > static_cast<int32>(BurstFired.size()))
		{
			BurstFired.resize(CurrentLODLevelIndex + 1);
		}

		FLODBurstFired& LocalBurstFired = BurstFired[CurrentLODLevelIndex];

		if (LocalBurstFired.Fired.size() < SpawnModule->BurstList.size())
		{
			LocalBurstFired.Fired.resize(SpawnModule->BurstList.size(), false);
		}

		// 중요:
		// LOD 전환 시 이미 시간이 지난 burst는 다시 터지면 안 되므로 fired 처리한다.
		for (int32 BurstIndex = 0; BurstIndex < SpawnModule->BurstList.size(); BurstIndex++)
		{
			if (CurrentLODLevel->RequiredModule->EmitterDelay + SpawnModule->BurstList[BurstIndex].Time < EmitterTime)
			{
				LocalBurstFired.Fired[BurstIndex] = true;
			}
		}
	}

	// Unreal은 game world에서만 disabled LOD particle을 죽인다.
	// 우리 엔진에 IsGameWorld 구분이 없으면 일단 true로 봐도 된다.
	if (Component->IsGameWorld() && !CurrentLODLevel->bEnabled)
	{
		KillParticlesForced(false);
	}
}

void FParticleEmitterInstance::Rewind()
{
	SecondsSinceCreation = 0;
	EmitterTime = 0;
	LoopCount = 0;
	ParticleCounter = 0;
	bEnabled = 1;
	ResetBurstList();
}

FBoundingBox FParticleEmitterInstance::GetBoundingBox() const
{
	return ParticleBoundingBox;
}

void FParticleEmitterInstance::FakeBursts()
{
	if (!CurrentLODLevel || !CurrentLODLevel->SpawnModule)
	{
		return;
	}

	if (CurrentLODLevelIndex < 0 ||
		CurrentLODLevelIndex >= static_cast<int32>(BurstFired.size()))
	{
		return;
	}

	FLODBurstFired& LocalBurstFired = BurstFired[CurrentLODLevelIndex];

	for (int32 BurstIndex = 0;
		BurstIndex < static_cast<int32>(CurrentLODLevel->SpawnModule->BurstList.size());
		++BurstIndex)
	{
		if (BurstIndex >= static_cast<int32>(LocalBurstFired.Fired.size()))
		{
			continue;
		}

		const FParticleBurst& Burst = CurrentLODLevel->SpawnModule->BurstList[BurstIndex];
		if (EmitterTime >= Burst.Time)
		{
			LocalBurstFired.Fired[BurstIndex] = true;
		}
	}
}

int32 FParticleEmitterInstance::GetOrbitPayloadOffset()
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (LODLevel->OrbitModules.empty())
	{
		return -1;
	}

	UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);
	assert(HighestLODLevel != nullptr);

	if (HighestLODLevel->OrbitModules.empty())
	{
		return -1;
	}

	const int32 LastOrbitIndex = static_cast<int32>(LODLevel->OrbitModules.size()) - 1;
	if (LastOrbitIndex < 0 || LastOrbitIndex >= static_cast<int32>(HighestLODLevel->OrbitModules.size()))
	{
		return -1;
	}

	UParticleModule* LastOrbit = HighestLODLevel->OrbitModules[LastOrbitIndex];
	auto It = SpriteTemplate->ModuleOffsetMap.find(LastOrbit);
	return It != SpriteTemplate->ModuleOffsetMap.end() ? static_cast<int32>(It->second) : -1;
}

FVector FParticleEmitterInstance::GetParticleLocationWithOrbitOffset(FBaseParticle* Particle)
{
	const int32 OrbitOffsetValue = GetOrbitPayloadOffset();
	if (OrbitOffsetValue == -1)
	{
		return Particle->Location;
	}

	const uint8* ParticleBase = reinterpret_cast<const uint8*>(Particle);
	const FOrbitChainModuleInstancePayload& OrbitPayload =
		*reinterpret_cast<const FOrbitChainModuleInstancePayload*>(ParticleBase + OrbitOffsetValue);
	return Particle->Location + OrbitPayload.Offset;
}

void FParticleEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel->RequiredModule != nullptr);

	ParticleBoundingBox = FBoundingBox();

	const FVector Scale = Component->GetWorldScale();
	const bool bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = bUseLocalSpace ? Component->GetWorldMatrix() : FMatrix::Identity;

	const int32 OrbitOffsetValue = GetOrbitPayloadOffset();
	const bool bSkipDoubleSpawnUpdate = SpriteTemplate ? !SpriteTemplate->bUseLegacySpawningBehavior : true;
	const FVector ParticlePivotOffset = FVector(-0.5f, -0.5f, 0.0f) + PivotOffset;

	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		Particle.OldLocation = Particle.Location;

		const bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		Particle.Flags &= ~STATE_Particle_JustSpawned;

		const bool bSkipUpdate = bJustSpawned && bSkipDoubleSpawnUpdate;

		FVector NewLocation = Particle.Location;
		float NewRotation = Particle.Rotation;

		if ((Particle.Flags & STATE_Particle_Freeze) == 0 && !bSkipUpdate)
		{
			if ((Particle.Flags & STATE_Particle_FreezeTranslation) == 0)
			{
				NewLocation = Particle.Location + Particle.Velocity * DeltaTime;
			}

			if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
				NewRotation = Particle.Rotation + Particle.RotationRate * DeltaTime;
			}
		}

		float LocalMax = 0.0f;
		if (OrbitOffsetValue == -1)
		{
			LocalMax = (Particle.Size * Scale).GetAbsMax();
		}
		else
		{
			int32 CurrentOffset = OrbitOffsetValue;
			uint8* ParticleBase = reinterpret_cast<uint8*>(&Particle);
			PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
			LocalMax = OrbitPayload.Offset.GetAbsMax();
		}
		LocalMax += (Particle.Size * ParticlePivotOffset).GetAbsMax();

		NewLocation += PositionOffsetThisTick;
		Particle.OldLocation += PositionOffsetThisTick;
		Particle.Location = NewLocation;
		Particle.Rotation = std::fmod(NewRotation, 6.28318530718f);

		FVector PositionForBounds = NewLocation;
		if (bUseLocalSpace)
		{
			PositionForBounds = ComponentToWorld.TransformPosition(NewLocation);
		}

		ParticleBoundingBox.Expand(PositionForBounds - FVector(LocalMax, LocalMax, LocalMax));
		ParticleBoundingBox.Expand(PositionForBounds + FVector(LocalMax, LocalMax, LocalMax));
	}
}


void FParticleEmitterInstance::ForceUpdateBoundingBox()
{
	if (ActiveParticles <= 0 || !ParticleData || !ParticleIndices)
	{
		ParticleBoundingBox = FBoundingBox();
		return;
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel->RequiredModule != nullptr);

	const FVector ComponentScale =
		Component->GetWorldScale();

	const bool bUseLocalSpace =
		LODLevel->RequiredModule->bUseLocalSpace;

	const FMatrix ComponentToWorld =
		bUseLocalSpace ? Component->GetWorldMatrix() : FMatrix::Identity;

	int32 OrbitOffsetValue = GetOrbitPayloadOffset();

	FVector MinVal(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxVal(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		float LocalMax = 0.0f;

		if (OrbitOffsetValue == -1)
		{
			const FVector ScaledSize(
				Particle.Size.X * ComponentScale.X,
				Particle.Size.Y * ComponentScale.Y,
				Particle.Size.Z * ComponentScale.Z);

			LocalMax = ScaledSize.GetAbsMax();;
		}
		else
		{
			const uint8* ParticleBase =
				reinterpret_cast<const uint8*>(&Particle);

			const FOrbitChainModuleInstancePayload& OrbitPayload =
				*reinterpret_cast<const FOrbitChainModuleInstancePayload*>(
					ParticleBase + OrbitOffsetValue);

			LocalMax = OrbitPayload.Offset.GetAbsMax();
		}
		FVector PositionForBounds = Particle.Location;

		if (bUseLocalSpace)
		{
			PositionForBounds =
				ComponentToWorld.TransformPositionWithW(Particle.Location);
		}

		MinVal.X = std::min(MinVal.X, PositionForBounds.X - LocalMax);
		MinVal.Y = std::min(MinVal.Y, PositionForBounds.Y - LocalMax);
		MinVal.Z = std::min(MinVal.Z, PositionForBounds.Z - LocalMax);

		MaxVal.X = std::max(MaxVal.X, PositionForBounds.X + LocalMax);
		MaxVal.Y = std::max(MaxVal.Y, PositionForBounds.Y + LocalMax);
		MaxVal.Z = std::max(MaxVal.Z, PositionForBounds.Z + LocalMax);
	}

	ParticleBoundingBox = FBoundingBox(MinVal, MaxVal);
}

uint32 FParticleEmitterInstance::RequiredBytes()
{
	assert(SpriteTemplate != nullptr);

	uint32 Bytes = 0;
	bool bHasSubUV = false;

	for (int32 LODIndex = 0; LODIndex < static_cast<int32>(SpriteTemplate->LODLevels.size()); ++LODIndex)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(LODIndex);
		assert(LODLevel != nullptr);
		assert(LODLevel->RequiredModule != nullptr);

		//if (LODLevel->RequiredModule->InterpolationMethod != PSUVIM_None)
		//{
		//	bHasSubUV = true;
		//}
	}

	//if (bHasSubUV)
	//{
	//	SubUVDataOffset = PayloadOffset;
	//	Bytes += sizeof(FFullSubUVPayload);
	//}
	//else
	//{
	//	SubUVDataOffset = 0;
	//}

	return Bytes;
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module) const
{
	if (!SpriteTemplate || !Module)
	{
		return 0;
	}

	auto It = SpriteTemplate->ModuleOffsetMap.find(Module);
	if (It == SpriteTemplate->ModuleOffsetMap.end())
	{
		return 0;
	}

	return It->second;
}

uint8* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* Module) const
{
	if (!SpriteTemplate || !Module || !InstanceData)
	{
		return nullptr;
	}

	auto It = SpriteTemplate->ModuleInstanceOffsetMap.find(Module);
	if (It == SpriteTemplate->ModuleInstanceOffsetMap.end())
	{
		return nullptr;
	}

	if (It->second >= static_cast<uint32>(InstancePayloadSize))
	{
		return nullptr;
	}

	return InstanceData + It->second;
}

FParticleRandomSeedInstancePayload* FParticleEmitterInstance::GetModuleRandomSeedInstanceData(UParticleModule* Module) const
{
	if (!SpriteTemplate || !Module || !InstanceData)
	{
		return nullptr;
	}

	auto It = SpriteTemplate->ModuleRandomSeedInstanceOffsetMap.find(Module);
	if (It == SpriteTemplate->ModuleRandomSeedInstanceOffsetMap.end())
	{
		return nullptr;
	}

	if (It->second >= static_cast<uint32>(InstancePayloadSize))
	{
		return nullptr;
	}

	return reinterpret_cast<FParticleRandomSeedInstancePayload*>(InstanceData + It->second);
}

uint8* FParticleEmitterInstance::GetTypeDataModuleInstanceData() const
{
	if (InstanceData && TypeDataInstanceOffset != -1)
	{
		return InstanceData + TypeDataInstanceOffset;
	}
	return nullptr;
}

uint32 FParticleEmitterInstance::CalculateParticleStride(uint32 InParticleSize)
{
	return InParticleSize;
}

void FParticleEmitterInstance::ResetBurstList()
{
	for (FLODBurstFired& LODBurstFired : BurstFired)
	{
		for (int32 i = 0; i < LODBurstFired.Fired.size(); ++i)
		{
			LODBurstFired.Fired[i] = false;
		}
	}
}

float FParticleEmitterInstance::GetCurrentBurstRateOffset(float& DeltaTime, int32& Burst)
{
	float SpawnRateInc = 0.0f;

	// Grab the current LOD level
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (LODLevel->SpawnModule->BurstList.size() > 0)
	{
		// For each burst in the list
		for (int32 BurstIdx = 0; BurstIdx < LODLevel->SpawnModule->BurstList.size(); BurstIdx++)
		{
			FParticleBurst* BurstEntry = &(LODLevel->SpawnModule->BurstList[BurstIdx]);
			// If it hasn't been fired
			if (BurstEntry && LODLevel->Level < BurstFired.size())
			{
				FLODBurstFired& LocalBurstFired = BurstFired[LODLevel->Level];
				if (BurstIdx < LocalBurstFired.Fired.size())
				{
					if (LocalBurstFired.Fired[BurstIdx] == false)
					{
						// If it is time to fire it
						if (EmitterTime >= BurstEntry->Time)
						{
							// Make sure there is a valid time slice
							if (DeltaTime < 0.00001f)
							{
								DeltaTime = 0.00001f;
							}
							// Calculate the increase time slice
							int32 Count = BurstEntry->Count;
							// KraftonEngine does not expose Cascade's per-emitter random stream yet.
							// CountLow and distribution BurstScale therefore cannot match UE exactly.
							// Take in to account scale.
							float Scale = LODLevel->SpawnModule->BurstScale;
							Count = static_cast<int32>(std::ceil(static_cast<float>(Count) * LODLevel->SpawnModule->BurstScale));
							SpawnRateInc += Count / DeltaTime;
							Burst += Count;
							LocalBurstFired.Fired[BurstIdx] = true;
						}
					}
				}
			}
		}
	}

	return SpawnRateInc;
}

void FParticleEmitterInstance::SetupEmitterDuration()
{
	assert(SpriteTemplate != nullptr);

	if (EmitterDurations.size() != SpriteTemplate->LODLevels.size())
	{
		EmitterDurations.clear();
		EmitterDurations.resize(SpriteTemplate->LODLevels.size(), 1.0f);
	}

	for (int32 LODIndex = 0; LODIndex < static_cast<int32>(SpriteTemplate->LODLevels.size()); ++LODIndex)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->GetLODLevel(LODIndex);
		assert(LODLevel != nullptr);
		assert(LODLevel->RequiredModule != nullptr);

		UParticleModuleRequired* Required = LODLevel->RequiredModule;
		// KraftonEngine currently lacks UE's per-required-module random stream and
		// component-level emitter delay, so range values use deterministic endpoints.
		CurrentDelay = std::max(0.0f, Required->EmitterDelay);
		float Duration = std::max(0.0f, Required->EmitterDuration);

		if (Required->EmitterDurationLow >= 0.0f && Required->EmitterDurationLow < Required->EmitterDuration)
		{
			// Phase1에는 distribution/random range UI가 없으므로 determinism을 위해 high value를 사용한다.
			Duration = Required->EmitterDuration;
		}

		EmitterDurations[LODIndex] = Duration + CurrentDelay;

		if ((LoopCount == 1) && Required->bDelayFirstLoopOnly &&
			(Required->EmitterLoops == 0 || Required->EmitterLoops > 1))
		{
			EmitterDurations[LODIndex] -= CurrentDelay;
		}
	}

	if (CurrentLODLevelIndex >= 0 && CurrentLODLevelIndex < static_cast<int32>(EmitterDurations.size()))
	{
		EmitterDuration = EmitterDurations[CurrentLODLevelIndex];
	}
}

void FParticleEmitterInstance::ResetParticleParameters(float DeltaTime)
{
	if (!ParticleData || !ParticleIndices)
	{
		return;
	}

	TArray<int32> OrbitOffsets;

	assert(CurrentLODLevel != nullptr);
	assert(SpriteTemplate != nullptr);

	if (!CurrentLODLevel->OrbitModules.empty())
	{
		for (UParticleModule* OrbitModule : CurrentLODLevel->OrbitModules)
		{
			if (!OrbitModule)
			{
				continue;
			}
			auto It = SpriteTemplate->ModuleOffsetMap.find(OrbitModule);
			if (It != SpriteTemplate->ModuleOffsetMap.end())
			{
				OrbitOffsets.push_back(static_cast<int32>(It->second));
			}
		}
	}

	const bool bSkipDoubleSpawnUpdate =
		SpriteTemplate ? !SpriteTemplate->bUseLegacySpawningBehavior : true;

	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ++ParticleIndex)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);

		Particle.Velocity = Particle.BaseVelocity;
		Particle.Size = GetParticleBaseSize(Particle);
		Particle.RotationRate = Particle.BaseRotationRate;
		Particle.Color = Particle.BaseColor;

		const bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		const bool bSkipUpdate = bJustSpawned && bSkipDoubleSpawnUpdate;

		if (!bSkipUpdate)
		{
			Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		}

		if (CameraPayloadOffset > 0)
		{
			int32 CurrentOffset = CameraPayloadOffset;
			const uint8* ParticleBase = (const uint8*)&Particle;
			PARTICLE_ELEMENT(FCameraOffsetParticlePayload, CameraOffsetPayload);
			CameraOffsetPayload.Offset = CameraOffsetPayload.BaseOffset;
		}

		for (int32 OrbitIndex = 0; OrbitIndex < OrbitOffsets.size(); OrbitIndex++)
		{
			int32 CurrentOffset = OrbitOffsets[OrbitIndex];
			const uint8* ParticleBase = (const uint8*)&Particle;
			PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
			OrbitPayload.PreviousOffset = OrbitPayload.Offset;
			OrbitPayload.Offset = OrbitPayload.BaseOffset;
			OrbitPayload.RotationRate = OrbitPayload.BaseRotationRate;
		}
	}
}

void FParticleEmitterInstance::CalculateOrbitOffset(
	FOrbitChainModuleInstancePayload& Payload,
	FVector& AccumOffset,
	FVector& AccumRotation,
	FVector& AccumRotationRate,
	float DeltaTime,
	FVector& Result,
	FMatrix& RotationMat)
{
	AccumRotation += AccumRotationRate * DeltaTime;
	Payload.Rotation = AccumRotation;

	if (!AccumRotation.IsNearlyZero())
	{
		// Cascade treats orbit rotation as turns and scales it to degrees. Our math layer uses Euler matrix,
		// so we keep the same policy conceptually and let the engine's matrix implementation decide units.
		const FVector ScaledRotation = RotationMat.TransformVector(AccumRotation) * 360.0f;
		FMatrix RotMat = FMatrix::MakeRotationEuler(ScaledRotation);
		RotationMat = RotationMat * RotMat;
		Result = RotationMat.TransformPosition(AccumOffset);
	}
	else
	{
		Result = AccumOffset;
	}

	AccumOffset = FVector::ZeroVector;
	AccumRotation = FVector::ZeroVector;
	AccumRotationRate = FVector::ZeroVector;
}

void FParticleEmitterInstance::UpdateOrbitData(float DeltaTime)
{
	/*UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel != nullptr);
	assert(SpriteTemplate != nullptr);

	const int32 ModuleCount = static_cast<int32>(LODLevel->OrbitModules.size());
	if (ModuleCount <= 0)
	{
		return;
	}

	UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);
	assert(HighestLODLevel != nullptr);

	TArray<FVector> Offsets;
	Offsets.resize(ModuleCount + 1, FVector::ZeroVector);

	TArray<int32> ModuleOffsets;
	ModuleOffsets.resize(ModuleCount + 1, 0);

	for (int32 ModOffIndex = 0; ModOffIndex < ModuleCount; ++ModOffIndex)
	{
		UParticleModule* HighestOrbitModule =
			ModOffIndex < static_cast<int32>(HighestLODLevel->OrbitModules.size())
			? HighestLODLevel->OrbitModules[ModOffIndex]
			: LODLevel->OrbitModules[ModOffIndex];

		if (HighestOrbitModule)
		{
			ModuleOffsets[ModOffIndex] =
				static_cast<int32>(GetModuleDataOffset(HighestOrbitModule));
		}
	}

	for (int32 i = ActiveParticles - 1; i >= 0; --i)
	{
		int32 OffsetIndex = 0;

		const int32 CurrentIndex = ParticleIndices[i];
		uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride;
		FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticleBase);

		if ((Particle.Flags & STATE_Particle_Freeze) != 0)
		{
			continue;
		}

		FVector AccumulatedOffset = FVector::ZeroVector;
		FVector AccumulatedRotation = FVector::ZeroVector;
		FVector AccumulatedRotationRate = FVector::ZeroVector;

		FOrbitChainModuleInstancePayload* LocalOrbitPayload = nullptr;
		FOrbitChainModuleInstancePayload* PrevOrbitPayload = nullptr;
		EOrbitChainMode PrevOrbitChainMode = EOChainMode_Add;

		FMatrix AccumRotMatrix = FMatrix::Identity;

		for (int32 OrbitIndex = 0; OrbitIndex < ModuleCount; ++OrbitIndex)
		{
			const int32 CurrentOffset = ModuleOffsets[OrbitIndex];

			UParticleModuleOrbit* OrbitModule =
				static_cast<UParticleModuleOrbit*>(LODLevel->OrbitModules[OrbitIndex]);

			if (!OrbitModule || CurrentOffset == 0)
			{
				continue;
			}

			FOrbitChainModuleInstancePayload& OrbitPayload =
				*reinterpret_cast<FOrbitChainModuleInstancePayload*>(
					ParticleBase + CurrentOffset);

			bool bCalculateOffset = false;

			if (OrbitIndex == ModuleCount - 1)
			{
				LocalOrbitPayload = &OrbitPayload;
				bCalculateOffset = true;
			}

			if (OrbitModule->ChainMode == EOChainMode_Add)
			{
				if (OrbitModule->bEnabled)
				{
					AccumulatedOffset += OrbitPayload.Offset;
					AccumulatedRotation += OrbitPayload.Rotation;
					AccumulatedRotationRate += OrbitPayload.RotationRate;
				}
			}
			else if (OrbitModule->ChainMode == EOChainMode_Scale)
			{
				if (OrbitModule->bEnabled)
				{
					AccumulatedOffset *= OrbitPayload.Offset;
					AccumulatedRotation *= OrbitPayload.Rotation;
					AccumulatedRotationRate *= OrbitPayload.RotationRate;
				}
			}
			else if (OrbitModule->ChainMode == EOChainMode_Link)
			{
				if ((OrbitIndex > 0) &&
					PrevOrbitChainMode == EOChainMode_Link &&
					PrevOrbitPayload)
				{
					FVector ResultOffset;
					CalculateOrbitOffset(
						*PrevOrbitPayload,
						AccumulatedOffset,
						AccumulatedRotation,
						AccumulatedRotationRate,
						DeltaTime,
						ResultOffset,
						AccumRotMatrix);

					if (!OrbitModule->bEnabled)
					{
						AccumulatedOffset = FVector::ZeroVector;
						AccumulatedRotation = FVector::ZeroVector;
						AccumulatedRotationRate = FVector::ZeroVector;
					}

					Offsets[OffsetIndex++] = ResultOffset;
				}

				if (OrbitModule->bEnabled)
				{
					AccumulatedOffset = OrbitPayload.Offset;
					AccumulatedRotation = OrbitPayload.Rotation;
					AccumulatedRotationRate = OrbitPayload.RotationRate;
				}
			}

			if (bCalculateOffset)
			{
				FVector ResultOffset;
				CalculateOrbitOffset(
					OrbitPayload,
					AccumulatedOffset,
					AccumulatedRotation,
					AccumulatedRotationRate,
					DeltaTime,
					ResultOffset,
					AccumRotMatrix);

				Offsets[OffsetIndex++] = ResultOffset;
			}

			if (OrbitModule->bEnabled)
			{
				PrevOrbitPayload = &OrbitPayload;
				PrevOrbitChainMode = OrbitModule->ChainMode;
			}
		}

		if (LocalOrbitPayload)
		{
			LocalOrbitPayload->Offset = FVector::ZeroVector;

			for (int32 AccumIndex = 0; AccumIndex < OffsetIndex; ++AccumIndex)
			{
				LocalOrbitPayload->Offset += Offsets[AccumIndex];
			}

			std::fill(Offsets.begin(), Offsets.end(), FVector::ZeroVector);
		}
	}*/
}

void FParticleEmitterInstance::ParticlePrefetch()
{
	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ParticleIndex++)
	{
		const std::uintptr_t Address =
			reinterpret_cast<std::uintptr_t>(this->ParticleData) +
			static_cast<std::uintptr_t>(this->ParticleStride) *
			static_cast<std::uintptr_t>(this->ParticleIndices[ParticleIndex]);

		_mm_prefetch(
			reinterpret_cast<const char*>(Address),
			_MM_HINT_T0);
	}
}

void FParticleEmitterInstance::CheckSpawnCount(int32 InNewCount, int32 InMaxCount)
{
	// UE reports this through world settings, screen debug messages, and particle stats.
	// KraftonEngine does not expose those hooks from the emitter instance layer yet.
	(void)InNewCount;
	(void)InMaxCount;
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel != nullptr);
	assert(SpriteTemplate != nullptr);

	float SpawnRate = 0.0f;
	int32 BurstCount = 0;
	float OldLeftover = SpawnFraction;

	if (SpriteTemplate->QualityLevelSpawnRateScale > 0.0f)
	{
		// KraftonEngine does not have UE's UParticleModuleSpawnBase stack or
		// distribution-based Rate/RateScale/GlobalRateScale evaluation yet.
		if (LODLevel->SpawnModule)
		{
			if (LODLevel->SpawnModule->bEnabled)
			{
				SpawnRate =
					std::max<float>(0.0f, LODLevel->SpawnModule->SpawnRate) *
					std::max<float>(0.0f, LODLevel->SpawnModule->SpawnRateScale);
			}

			int32 Burst = 0;
			GetCurrentBurstRateOffset(DeltaTime, Burst);
			BurstCount += Burst;
		}

		const float QualityMult = SpriteTemplate->GetQualityLevelSpawnRateMult();

		SpawnRate = std::max<float>(0.0f, SpawnRate * QualityMult);
		BurstCount = static_cast<int32>(std::ceil(static_cast<float>(BurstCount) * QualityMult));
	}
	else
	{
		SpawnRate = 0.0f;
		BurstCount = 0;
	}

	if ((SpawnRate > 0.0f) || (BurstCount > 0))
	{
		float SafetyLeftover = OldLeftover;

		float NewLeftover = OldLeftover + DeltaTime * SpawnRate;
		int32 Number = static_cast<int32>(std::floor(NewLeftover));
		float Increment = (SpawnRate > 0.0f) ? (1.0f / SpawnRate) : 0.0f;
		float StartTime = DeltaTime + OldLeftover * Increment - Increment;

		NewLeftover = NewLeftover - static_cast<float>(Number);

		bool bProcessSpawn = true;
		int32 NewCount = ActiveParticles + Number + BurstCount;

		const int32 MaxCPUParticlesPerEmitter =
			std::numeric_limits<uint16>::max();

		if (NewCount > MaxCPUParticlesPerEmitter)
		{
			int32 MaxNewParticles =
				MaxCPUParticlesPerEmitter - ActiveParticles;

			BurstCount = std::min(MaxNewParticles, BurstCount);
			MaxNewParticles -= BurstCount;

			Number = std::min(MaxNewParticles, Number);

			NewCount = ActiveParticles + Number + BurstCount;
		}

		const float BurstIncrement =
			(SpriteTemplate->bUseLegacySpawningBehavior && BurstCount > 0)
			? (1.0f / static_cast<float>(BurstCount))
			: 0.0f;

		const float BurstStartTime =
			SpriteTemplate->bUseLegacySpawningBehavior
			? DeltaTime * BurstIncrement
			: 0.0f;

		if (NewCount >= MaxActiveParticles)
		{
			const int32 Slack =
				static_cast<int32>(
					std::sqrt(std::sqrt(static_cast<float>(std::max(NewCount, 1)))) + 1.0f);

			if (DeltaTime < PeakActiveParticleUpdateDelta)
			{
				bProcessSpawn = Resize(NewCount + Slack);
			}
			else
			{
				bProcessSpawn = Resize(NewCount + Slack, false);
			}
		}

		if (bProcessSpawn == true)
		{
			const FVector InitialLocation = EmitterToSimulation.GetOrigin();

			SpawnParticles(
				Number,
				StartTime,
				Increment,
				InitialLocation,
				FVector::ZeroVector,
				nullptr);

			SpawnParticles(
				BurstCount,
				BurstStartTime,
				BurstIncrement,
				InitialLocation,
				FVector::ZeroVector,
				nullptr);

			return NewLeftover;
		}

		return SafetyLeftover;
	}

	return SpawnFraction;
}

void FParticleEmitterInstance::FixupParticleIndices()
{
	if (!ParticleIndices || MaxActiveParticles <= 0)
	{
		ActiveParticles = 0;
		return;
	}

	ActiveParticles = std::max(0, std::min(ActiveParticles, MaxActiveParticles));

	TArray<uint8> Used;
	Used.resize(MaxActiveParticles, 0);

	TArray<uint16> NewIndices;
	NewIndices.reserve(MaxActiveParticles + 1);

	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const uint16 Index = ParticleIndices[i];
		if (Index < MaxActiveParticles && Used[Index] == 0)
		{
			Used[Index] = 1;
			NewIndices.push_back(Index);
		}
	}

	ActiveParticles = static_cast<int32>(NewIndices.size());

	for (int32 i = 0; i < MaxActiveParticles; ++i)
	{
		if (Used[i] == 0)
		{
			NewIndices.push_back(static_cast<uint16>(i));
		}
	}

	for (int32 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = NewIndices[i];
	}

	ParticleIndices[MaxActiveParticles] = static_cast<uint16>(MaxActiveParticles - 1);
}

void FParticleEmitterInstance::SpawnParticles(
	int32 Count,
	float StartTime,
	float Increment,
	const FVector& InitialLocation,
	const FVector& InitialVelocity,
	FParticleEventInstancePayload* EventPayload)
{
	if (Count <= 0)
	{
		return;
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel != nullptr);
	assert(SpriteTemplate != nullptr);

	assert(ActiveParticles <= MaxActiveParticles);

	if (ActiveParticles + Count > MaxActiveParticles)
	{
		const int32 DesiredMax =
			std::max(ActiveParticles + Count, std::max(32, MaxActiveParticles * 2));
		Resize(DesiredMax);
	}

	Count = std::min(Count, MaxActiveParticles - ActiveParticles);
	if (Count <= 0)
	{
		return;
	}

	auto SpawnInternal = [&](bool bLegacySpawnBehavior)
		{
			UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);
			assert(HighestLODLevel != nullptr);
			float SpawnTime = StartTime;
			float Interp = 1.0f;
			const float InterpIncrement =
				(Count > 0 && Increment > 0.0f) ? (1.0f / static_cast<float>(Count)) : 0.0f;

			for (int32 i = 0; i < Count; ++i)
			{
				if (!ParticleData || !ParticleIndices)
				{
					ActiveParticles = 0;
					break;
				}

				if (ActiveParticles >= MaxActiveParticles)
				{
					break;
				}

				uint16 NextFreeIndex = ParticleIndices[ActiveParticles];
				if (NextFreeIndex >= MaxActiveParticles)
				{
					FixupParticleIndices();
					if (ActiveParticles >= MaxActiveParticles)
					{
						break;
					}
					NextFreeIndex = ParticleIndices[ActiveParticles];
					if (NextFreeIndex >= MaxActiveParticles)
					{
						break;
					}
				}

				DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);
				const int32 CurrentParticleIndex = ActiveParticles++;

				if (bLegacySpawnBehavior)
				{
					SpawnTime -= Increment;
					Interp -= InterpIncrement;
				}

				PreSpawn(Particle, InitialLocation, InitialVelocity);

				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LODLevel->SpawnModules.size()); ++ModuleIndex)
				{
					UParticleModule* SpawnModule = LODLevel->SpawnModules[ModuleIndex];
					if (!SpawnModule || !SpawnModule->bEnabled)
					{
						continue;
					}

					assert(ModuleIndex < static_cast<int32>(HighestLODLevel->SpawnModules.size()));
					UParticleModule* OffsetModule = HighestLODLevel->SpawnModules[ModuleIndex];
					assert(OffsetModule != nullptr);

					SpawnModule->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(OffsetModule)), SpawnTime, Particle });
				}

				PostSpawn(Particle, Interp, SpawnTime);

				if (Particle->Location.ContainsNaN() || Particle->RelativeTime > 1.0f)
				{
					KillParticle(CurrentParticleIndex);
					continue;
				}

				// KraftonEngine does not currently have UE's EventGenerator dispatch path.
				if (EventPayload && EventPayload->bSpawnEventsPresent)
				{
					++EventPayload->SpawnTrackingCount;
				}

				if (!bLegacySpawnBehavior)
				{
					SpawnTime -= Increment;
					Interp -= InterpIncrement;
				}
			}
		};

	SpawnInternal(SpriteTemplate && SpriteTemplate->bUseLegacySpawningBehavior);
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLODLevelChecked() const
{
	assert(SpriteTemplate != nullptr);

	UParticleLODLevel* LODLevel =
		CurrentLODLevel ? CurrentLODLevel : SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
	assert(LODLevel != nullptr);
	assert(LODLevel->RequiredModule != nullptr);
	return LODLevel;
}

void FParticleEmitterInstance::ForceSpawn(
	float DeltaTime,
	int32 InSpawnCount,
	int32 InBurstCount,
	FVector& InLocation,
	FVector& InVelocity)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel->RequiredModule != nullptr);

	// 원본 구조 보존:
	// ForceSpawn은 distribution 기반 SpawnRate를 보지 않고,
	// 외부에서 들어온 SpawnCount / BurstCount를 그대로 처리한다.
	int32 SpawnCount = InSpawnCount;
	int32 BurstCount = InBurstCount;

	// 원본에 있는 변수지만 현재 함수 안에서는 실질적으로 사용되지 않는다.
	// 원본 흐름 보존을 위해 남긴다.
	float SpawnRateDivisor = 0.0f;
	float OldLeftover = 0.0f;

	UParticleLODLevel* HighestLODLevel =
		SpriteTemplate->GetLODLevel(0);
	assert(HighestLODLevel != nullptr);

	bool bProcessSpawnRate = true;
	bool bProcessBurstList = true;

	if ((SpawnCount > 0) || (BurstCount > 0))
	{
		int32 Number = SpawnCount;

		float Increment =
			(SpawnCount > 0)
			? (DeltaTime / static_cast<float>(SpawnCount))
			: 0.0f;

		float StartTime = DeltaTime;

		bool bProcessSpawn = true;

		int32 NewCount =
			ActiveParticles + Number + BurstCount;

		if (NewCount >= MaxActiveParticles)
		{
			const int32 Slack =
				static_cast<int32>(
					std::sqrt(std::sqrt(static_cast<float>(std::max(NewCount, 1)))) + 1.0f);

			if (DeltaTime < PeakActiveParticleUpdateDelta)
			{
				bProcessSpawn = Resize(NewCount + Slack);
			}
			else
			{
				bProcessSpawn = Resize(NewCount + Slack, false);
			}
		}

		if (bProcessSpawn == true)
		{
			// 원본 주석의 의미:
			// 기존 동작은 local-space일 때도 InLocation/InVelocity를 그대로 넘긴다.
			// 하지만 인터페이스 관점에서는 world-space 입력을 받아 local-space로 변환하는 쪽이 더 자연스럽다는 설명이다.
			const bool bUseLocalSpace =
				LODLevel->RequiredModule->bUseLocalSpace;

			[[maybe_unused]] FVector SpawnLocation =
				bUseLocalSpace ? FVector::ZeroVector : InLocation;

			[[maybe_unused]] FVector SpawnVelocity =
				bUseLocalSpace ? FVector::ZeroVector : InVelocity;

			// 원본 동작 보존:
			// 위에서 SpawnLocation/SpawnVelocity를 계산하지만,
			// 실제 SpawnParticles에는 InLocation/InVelocity를 그대로 넘긴다.
			SpawnParticles(
				Number,
				StartTime,
				Increment,
				InLocation,
				InVelocity,
				nullptr);

			SpawnParticles(
				BurstCount,
				StartTime,
				0.0f,
				InLocation,
				InVelocity,
				nullptr);
		}
	}
}

void FParticleEmitterInstance::PreSpawn(
	FBaseParticle* Particle,
	const FVector& InitialLocation,
	const FVector& InitialVelocity)
{
	assert(Particle != nullptr);
	assert(ParticleSize > 0);
	assert((ParticleSize % 16) == 0);

	std::memset(Particle, 0, static_cast<size_t>(ParticleSize));

	Particle->Location = InitialLocation - PositionOffsetThisTick;
	Particle->OldLocation = Particle->Location;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
	Particle->BaseSize = FVector::OneVector;
	Particle->Size = FVector::OneVector;
	Particle->BaseColor = FLinearColor::White();
	Particle->Color = FLinearColor::White();
	Particle->Rotation = 0.0f;
	Particle->BaseRotationRate = 0.0f;
	Particle->RotationRate = 0.0f;
	Particle->RelativeTime = 0.0f;
	Particle->OneOverMaxLifetime = 1.0f;
}

bool FParticleEmitterInstance::HasCompleted() const
{
	assert(SpriteTemplate != nullptr);

	// If it hasn't finished looping or if it loops forever, not completed.
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(LODLevel->RequiredModule != nullptr);

	if ((LODLevel->RequiredModule->EmitterLoops == 0) ||
		(SecondsSinceCreation < (EmitterDuration * static_cast<float>(LODLevel->RequiredModule->EmitterLoops))))
	{
		return false;
	}

	// If there are active particles, not completed.
	if (ActiveParticles > 0)
	{
		return false;
	}

	return true;
}

void FParticleEmitterInstance::Spawn(float OldLeftover, float Rate, float DeltaTime, int32 Burst, float BurstTime)
{
	const float ParticlesToSpawnFloat = Rate * DeltaTime + OldLeftover;
	const int32 Number = static_cast<int32>(std::floor(ParticlesToSpawnFloat));

	if (Number > 0)
	{
		const float Increment = Rate > 0.0f ? 1.0f / Rate : 0.0f;
		const float StartTime = DeltaTime + OldLeftover * Increment - Increment;
		SpawnParticles(Number, StartTime, Increment, Location, FVector::ZeroVector, nullptr);
	}

	if (Burst > 0)
	{
		SpawnParticles(Burst, BurstTime, 0.0f, Location, FVector::ZeroVector, nullptr);
	}
}

void FParticleEmitterInstance::PostSpawn(
	FBaseParticle* Particle,
	float InterpolationPercentage,
	float SpawnTime)
{
	assert(Particle != nullptr);

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	if (!LODLevel->RequiredModule->bUseLocalSpace)
	{
		if (FVector::DistSquared(OldLocation, Location) > 1.0f)
		{
			Particle->Location += (OldLocation - Location) * InterpolationPercentage;
		}
	}

	Particle->OldLocation = Particle->Location;
	Particle->Location += Particle->Velocity * SpawnTime;
	Particle->Flags |= static_cast<int32>(ParticleCounter & STATE_CounterMask);
	Particle->Flags |= STATE_Particle_JustSpawned;
	++ParticleCounter;
}

void FParticleEmitterInstance::KillParticles()
{
	if (!ParticleData || !ParticleIndices)
	{
		return;
	}

	bool bFoundCorruptIndices = false;

	for (int32 i = ActiveParticles - 1; i >= 0; --i)
	{
		const int32 CurrentIndex = ParticleIndices[i];
		if (CurrentIndex < 0 || CurrentIndex >= MaxActiveParticles)
		{
			bFoundCorruptIndices = true;
			continue;
		}

		DECLARE_PARTICLE(
			Particle,
			ParticleData + CurrentIndex * ParticleStride);

		if (Particle.RelativeTime > 1.0f)
		{
			ParticleIndices[i] = ParticleIndices[ActiveParticles - 1];
			ParticleIndices[ActiveParticles - 1] = static_cast<uint16>(CurrentIndex);
			--ActiveParticles;
		}
	}

	if (bFoundCorruptIndices)
	{
		FixupParticleIndices();
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	const uint16 KillIndex = ParticleIndices[Index];

	for (int32 i = Index; i < ActiveParticles - 1; ++i)
	{
		ParticleIndices[i] = ParticleIndices[i + 1];
	}

	ParticleIndices[ActiveParticles - 1] = KillIndex;
	--ActiveParticles;
}

void FParticleEmitterInstance::KillParticlesForced(bool bFireEvents)
{
	if (bFireEvents)
	{
		EventCount += ActiveParticles;
	}

	ActiveParticles = 0;

	if (!ParticleIndices)
	{
		return;
	}

	for (int32 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
	}

	if (MaxActiveParticles > 0)
	{
		ParticleIndices[MaxActiveParticles] = static_cast<uint16>(MaxActiveParticles - 1);
	}

	ParticleCounter = 0;
}


FBaseParticle* FParticleEmitterInstance::GetParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return nullptr;
	}

	DECLARE_PARTICLE_PTR(
		Particle,
		ParticleData + ParticleStride * ParticleIndices[Index]);

	return Particle;
}

FBaseParticle* FParticleEmitterInstance::GetParticleDirect(int32 DirectIndex)
{
	if (DirectIndex < 0 || DirectIndex >= MaxActiveParticles)
	{
		return nullptr;
	}

	DECLARE_PARTICLE_PTR(
		Particle,
		ParticleData + ParticleStride * DirectIndex);

	return Particle;
}



void FParticleEmitterInstance::ProcessParticleEvents(float DeltaTime, bool bSuppressSpawning)
{
	// KraftonEngine does not currently expose UE's component event arrays or event receiver modules.
	// Keep the entry point so Cascade-style receivers can be wired here later.
	(void)DeltaTime;
	(void)bSuppressSpawning;
}

void FParticleEmitterInstance::Tick_MaterialOverrides(int32 EmitterIndex)
{
	// KraftonEngine does not currently expose UE's NamedMaterialOverrides / NamedMaterialSlots.
	// Keep the emitter-index override path equivalent to UE's fallback branch.
	if (Component)
	{
		const TArray<UMaterial*>& EmitterMaterials = Component->GetEmitterMaterials();
		if (EmitterIndex >= 0 &&
			EmitterIndex < static_cast<int32>(EmitterMaterials.size()) &&
			EmitterMaterials[EmitterIndex])
		{
			CurrentMaterial = EmitterMaterials[EmitterIndex];
			return;
		}
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	LODLevel->RequiredModule->ResolveMaterialFromSlot();
	CurrentMaterial = LODLevel->RequiredModule->Material;
}

bool FParticleEmitterInstance::UseLocalSpace()
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	return LODLevel->RequiredModule->bUseLocalSpace;
}

void FParticleEmitterInstance::GetScreenAlignmentAndScale(int32& OutScreenAlign, FVector& OutScale)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	OutScreenAlign = LODLevel->RequiredModule->ScreenAlignment;
	OutScale = Component->GetWorldScale();
}

UMaterial* FParticleEmitterInstance::GetCurrentMaterial()
{
	UMaterial* RenderMaterial = CurrentMaterial;
	if (!RenderMaterial)
	{
		RenderMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");
	}

	// UE also validates MATUSAGE_ParticleSprites / MATUSAGE_MeshParticles here.
	// KraftonEngine's material system has no equivalent usage flag yet, so fallback only handles null.
	CurrentMaterial = RenderMaterial;
	return RenderMaterial;
}

bool FParticleEmitterInstance::IsDynamicDataRequired() const
{
	if (ActiveParticles <= 0 || !bEnabled || ParticleData == nullptr || ParticleIndices == nullptr)
	{
		return false;
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (!LODLevel->bEnabled)
	{
		return false;
	}

	if (LODLevel->RequiredModule->bUseMaxDrawCount &&
		LODLevel->RequiredModule->MaxDrawCount == 0)
	{
		return false;
	}

	return true;
}

bool FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (!IsDynamicDataRequired())
	{
		return false;
	}

	// Render replay data is consumed after simulation, so carry the current
	// simulation-to-world transform with the copied particle payloads.
	UpdateTransforms();

	const bool bLocalSpace = UseLocalSpace();
	OutData.SimulationToWorld = bLocalSpace ? SimulationToWorld : FMatrix::Identity;

	if (OutData.eEmitterType == EDynamicEmitterType::Sprite ||
		OutData.eEmitterType == EDynamicEmitterType::Mesh ||
		OutData.eEmitterType == EDynamicEmitterType::Beam ||
		OutData.eEmitterType == EDynamicEmitterType::Ribbon)
	{
		static_cast<FDynamicSpriteEmitterReplayDataBase&>(OutData).bUseLocalSpace = bLocalSpace;
	}

	CopyActiveParticlesToReplay(*this, OutData);
	return true;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData(bool bSelected)
{
	return nullptr;
}

FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData(bool bSelected)
{
	if (!IsDynamicDataRequired())
	{
		return nullptr;
	}

	FDynamicSpriteEmitterData* Data = new FDynamicSpriteEmitterData();
	const bool bValid = FillReplayData(Data->Source);

	if (!bValid)
	{
		delete Data;
		return nullptr;
	}

	return Data;
}

bool FParticleSpriteEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (!IsReplayType(OutData, EDynamicEmitterType::Sprite))
	{
		return false;
	}

	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return false;
	}

	FDynamicSpriteEmitterReplayDataBase& SpriteData =
		static_cast<FDynamicSpriteEmitterReplayDataBase&>(OutData);

	SpriteData.Material = GetCurrentMaterial();
	SpriteData.SubUVDataOffset = SubUVDataOffset;
	SpriteData.DynamicParameterDataOffset = DynamicParameterDataOffset;
	SpriteData.LightDataOffset = LightDataOffset;
	SpriteData.OrbitModuleOffset = OrbitModuleOffset;
	SpriteData.CameraPayloadOffset = CameraPayloadOffset;
	SpriteData.bLockAxis = bAxisLockEnabled;
	SpriteData.PivotOffset = PivotOffset;
	SpriteData.bUseLocalSpace = GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;
	SpriteData.SubImages_Horizontal = GetCurrentLODLevelChecked()->RequiredModule ? GetCurrentLODLevelChecked()->RequiredModule->SubImages_Horizontal : 1;
	SpriteData.SubImages_Vertical = GetCurrentLODLevelChecked()->RequiredModule ? GetCurrentLODLevelChecked()->RequiredModule->SubImages_Vertical : 1;

	return true;
}

void FParticleMeshEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent);
	MeshTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule) : nullptr;
	bMeshRotationActive = InTemplate ? InTemplate->bMeshRotationActive : false;
	bMotionBlurEnabled = MeshTypeData ? MeshTypeData->IsMotionBlurEnabled() : false;
}

void FParticleMeshEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();
	MeshTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule) : MeshTypeData;
}

bool FParticleMeshEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	const int32 OldMaxActiveParticles = MaxActiveParticles;
	if (FParticleEmitterInstance::Resize(NewMaxActiveParticles, bSetMaxActiveCount))
	{
		if (bMeshRotationActive && MeshRotationOffset > 0)
		{
			for (int32 i = OldMaxActiveParticles; i < NewMaxActiveParticles; ++i)
			{
				DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
				FMeshRotationPayloadData* Payload =
					reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshRotationOffset);
				Payload->RotationRateBase = FVector::ZeroVector;
			}
		}
		return true;
	}
	return false;
}

uint32 FParticleMeshEmitterInstance::RequiredBytes()
{
	uint32 Bytes = FParticleEmitterInstance::RequiredBytes();

	// UE always reserves mesh rotation payload for mesh emitters. The active flag
	// only controls per-frame rotation processing; mesh rotation modules and
	// motion blur still rely on this payload layout being stable.
	MeshRotationOffset = PayloadOffset + static_cast<int32>(Bytes);
	Bytes += sizeof(FMeshRotationPayloadData);

	if (bMotionBlurEnabled)
	{
		MeshMotionBlurOffset = PayloadOffset + static_cast<int32>(Bytes);
		Bytes += sizeof(FMeshMotionBlurPayloadData);
	}
	else
	{
		MeshMotionBlurOffset = 0;
	}

	return Bytes;
}


void FParticleMeshEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (bEnabled && MeshMotionBlurOffset > 0)
	{
		for (int32 i = 0; i < ActiveParticles; ++i)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			FMeshRotationPayloadData* RotationPayload =
				MeshRotationOffset > 0 ? reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshRotationOffset) : nullptr;
			FMeshMotionBlurPayloadData* MotionBlurPayload =
				reinterpret_cast<FMeshMotionBlurPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshMotionBlurOffset);

			MotionBlurPayload->BaseParticlePrevRotation = Particle.Rotation;
			MotionBlurPayload->BaseParticlePrevVelocity = Particle.Velocity;
			MotionBlurPayload->BaseParticlePrevSize = Particle.Size;
			MotionBlurPayload->PayloadPrevRotation = RotationPayload ? RotationPayload->Rotation : FVector::ZeroVector;

			if (CameraPayloadOffset > 0)
			{
				const FCameraOffsetParticlePayload* CameraPayload =
					reinterpret_cast<const FCameraOffsetParticlePayload*>(reinterpret_cast<const uint8*>(&Particle) + CameraPayloadOffset);
				MotionBlurPayload->PayloadPrevCameraOffset = CameraPayload->Offset;
			}
			else
			{
				MotionBlurPayload->PayloadPrevCameraOffset = 0.0f;
			}

			const int32 OrbitOffset = GetOrbitPayloadOffset();
			if (OrbitOffset != -1)
			{
				const FOrbitChainModuleInstancePayload* OrbitPayload =
					reinterpret_cast<const FOrbitChainModuleInstancePayload*>(reinterpret_cast<const uint8*>(&Particle) + OrbitOffset);
				MotionBlurPayload->PayloadPrevOrbitOffset = OrbitPayload->Offset;
			}
			else
			{
				MotionBlurPayload->PayloadPrevOrbitOffset = FVector::ZeroVector;
			}
		}
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (bMeshRotationActive && bEnabled && MeshRotationOffset > 0)
	{
		for (int32 i = 0; i < ActiveParticles; ++i)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			FMeshRotationPayloadData* Payload =
				reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshRotationOffset);
			Payload->RotationRate = Payload->RotationRateBase;

			if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity ||
				LODLevel->RequiredModule->ScreenAlignment == PSA_AwayFromCenter)
			{
				FVector NewDirection = Particle.Velocity;

				if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity)
				{
					// UE original responsibility: only apply orbit payloads when
					// RequiredModule->bOrbitModuleAffectsVelocityAlignment is true.
					// Missing Jungle foundation: that RequiredModule flag is not exposed yet.
					// System to connect later: RequiredModule orbit-alignment flag plus
					// the highest-LOD orbit module offset lookup.
				}
				else if (LODLevel->RequiredModule->ScreenAlignment == PSA_AwayFromCenter)
				{
					NewDirection = Particle.Location;
				}

				NewDirection = NewDirection.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
				const FQuat Rotation = FindBetweenNormals(FVector::XAxisVector, NewDirection);
				const FVector Euler = Rotation.ToRotator().ToVector();
				Payload->Rotation = Payload->InitRotation + Euler;
				Payload->Rotation += Payload->CurContinuousRotation;
			}
			else if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
				Payload->Rotation = Payload->InitRotation + Payload->CurContinuousRotation;
			}
		}
	}

	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);

	if (bMeshRotationActive && bEnabled && MeshRotationOffset > 0)
	{
		for (int32 i = 0; i < ActiveParticles; ++i)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			FMeshRotationPayloadData* Payload =
				reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshRotationOffset);
			Payload->CurContinuousRotation += Payload->RotationRate * DeltaTime;
		}
	}
}

void FParticleMeshEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	// UE original responsibility: include static mesh bounds transformed by mesh rotation,
	// camera-facing and axis-lock options.
	// Missing Jungle foundation: UStaticMesh render/physics bounds on particle TypeData.
	// System to connect later: StaticMesh bounds adapter, then keep UE UpdateBoundingBox order.
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);
}

void FParticleMeshEmitterInstance::PostSpawn(
	FBaseParticle* Particle,
	float InterpolationPercentage,
	float SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, SpawnTime);
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	uint8* ParticleBase = reinterpret_cast<uint8*>(Particle);
	if (MeshRotationOffset > 0)
	{
		FMeshRotationPayloadData& RotationPayload =
			*reinterpret_cast<FMeshRotationPayloadData*>(ParticleBase + MeshRotationOffset);

		if (LODLevel->RequiredModule->ScreenAlignment == PSA_Velocity ||
			LODLevel->RequiredModule->ScreenAlignment == PSA_AwayFromCenter)
		{
			FVector NewDirection = Particle->Velocity;
			if (LODLevel->RequiredModule->ScreenAlignment == PSA_AwayFromCenter)
			{
				NewDirection = Particle->Location;
			}

			NewDirection = NewDirection.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
			const FQuat Rotation = FindBetweenNormals(FVector::XAxisVector, NewDirection);
			const FVector Euler = Rotation.ToRotator().ToVector();
			RotationPayload.Rotation.X += Euler.X;
			RotationPayload.Rotation.Y += Euler.Y;
			RotationPayload.Rotation.Z += Euler.Z;
		}

		RotationPayload.InitialOrientation = MeshTypeData ? MeshTypeData->RollPitchYawRange.GetValue(SpawnTime, Component) : FVector::ZeroVector;

		// UE original responsibility: velocity/away-from-center alignment, camera-facing,
		// axis-lock, bApplyParticleRotationAsSpin and bFaceCameraDirectionRatherThanPosition.
		// Missing Jungle foundation: view/camera-facing basis calculation in mesh particle
		// dynamic path.
		// System to connect later: port UE mesh orientation helper at render-replay boundary.
	}

	if (MeshMotionBlurOffset > 0)
	{
		const FMeshRotationPayloadData* RotationPayload =
			MeshRotationOffset > 0 ? reinterpret_cast<const FMeshRotationPayloadData*>(ParticleBase + MeshRotationOffset) : nullptr;
		FMeshMotionBlurPayloadData& MotionBlurPayload =
			*reinterpret_cast<FMeshMotionBlurPayloadData*>(ParticleBase + MeshMotionBlurOffset);
		MotionBlurPayload.BaseParticlePrevVelocity = Particle->Velocity;
		MotionBlurPayload.BaseParticlePrevSize = Particle->Size;
		MotionBlurPayload.BaseParticlePrevRotation = Particle->Rotation;
		MotionBlurPayload.PayloadPrevRotation = RotationPayload ? RotationPayload->Rotation : FVector::ZeroVector;

		if (CameraPayloadOffset > 0)
		{
			const FCameraOffsetParticlePayload* CameraPayload =
				reinterpret_cast<const FCameraOffsetParticlePayload*>(ParticleBase + CameraPayloadOffset);
			MotionBlurPayload.PayloadPrevCameraOffset = CameraPayload->Offset;
		}
		else
		{
			MotionBlurPayload.PayloadPrevCameraOffset = 0.0f;
		}

		const int32 OrbitOffset = GetOrbitPayloadOffset();
		if (OrbitOffset != -1)
		{
			const FOrbitChainModuleInstancePayload* OrbitPayload =
				reinterpret_cast<const FOrbitChainModuleInstancePayload*>(ParticleBase + OrbitOffset);
			MotionBlurPayload.PayloadPrevOrbitOffset = OrbitPayload->Offset;
		}
		else
		{
			MotionBlurPayload.PayloadPrevOrbitOffset = FVector::ZeroVector;
		}
	}
}

void FParticleMeshEmitterInstance::Tick_MaterialOverrides(int32 EmitterIndex)
{
	FParticleEmitterInstance::Tick_MaterialOverrides(EmitterIndex);
	if (!Component)
	{
		return;
	}

	const TArray<UMaterial*>& EmitterMaterials = Component->GetEmitterMaterials();
	if (EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(EmitterMaterials.size()) ||
		!EmitterMaterials[EmitterIndex])
	{
		return;
	}

	if (CurrentMaterials.empty())
	{
		CurrentMaterials.push_back(EmitterMaterials[EmitterIndex]);
	}
	else
	{
		for (UMaterial*& Material : CurrentMaterials)
		{
			Material = EmitterMaterials[EmitterIndex];
		}
	}
}

void FParticleMeshEmitterInstance::SetMeshMaterials(const TArray<UMaterial*>& InMaterials)
{
	CurrentMaterials = InMaterials;
}

void FParticleMeshEmitterInstance::GetMeshMaterials(TArray<UMaterial*>& OutMaterials, const UParticleLODLevel* LODLevel, bool bLogWarnings) const
{
	OutMaterials.clear();
	const UStaticMesh* StaticMesh = MeshTypeData ? MeshTypeData->Mesh : nullptr;
	const TArray<FStaticMeshSection>* Sections = StaticMesh ? &StaticMesh->GetLODSections(0) : nullptr;
	const TArray<FStaticMaterial>* StaticMaterials = StaticMesh ? &StaticMesh->GetStaticMaterials() : nullptr;
	const int32 SectionCount = Sections && !Sections->empty() ? static_cast<int32>(Sections->size()) : 1;

	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		UMaterial* Material = nullptr;

		if (SectionIndex < static_cast<int32>(CurrentMaterials.size()))
		{
			Material = CurrentMaterials[SectionIndex];
		}

		if (!Material && LODLevel)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				UParticleModuleMeshMaterial* MeshMaterialModule = Cast<UParticleModuleMeshMaterial>(Module);
				if (MeshMaterialModule && MeshMaterialModule->bEnabled &&
					SectionIndex < static_cast<int32>(MeshMaterialModule->MeshMaterials.size()))
				{
					Material = MeshMaterialModule->MeshMaterials[SectionIndex];
					break;
				}
			}
		}

		if (!Material && MeshTypeData && MeshTypeData->bOverrideMaterial)
		{
			Material = CurrentMaterial ? CurrentMaterial : (LODLevel && LODLevel->RequiredModule ? LODLevel->RequiredModule->Material : nullptr);
		}

		if (!Material && Sections && StaticMaterials && SectionIndex < static_cast<int32>(Sections->size()))
		{
			const int32 MaterialIndex = (*Sections)[SectionIndex].MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(StaticMaterials->size()))
			{
				Material = (*StaticMaterials)[MaterialIndex].MaterialInterface;
			}
		}

		if (!Material)
		{
			Material = CurrentMaterial ? CurrentMaterial : FMaterialManager::Get().GetOrCreateMaterial("None");
		}

		OutMaterials.push_back(Material);
	}
}

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
	UStaticMesh* StaticMesh = MeshTypeData ? MeshTypeData->GetStaticMesh() : nullptr;
	if (!StaticMesh || !IsDynamicDataRequired())
	{
		return nullptr;
	}

	FDynamicMeshEmitterData* Data = new FDynamicMeshEmitterData();
	const bool bValid = FillReplayData(Data->Source);

	if (!bValid)
	{
		delete Data;
		return nullptr;
	}

	// UE FDynamicMeshEmitterData::Init selects the StaticMesh LOD after replay
	// data is filled. Jungle currently lacks view-dependent mesh-particle LOD
	// selection, so this adapter uses LOD0 until bUseStaticMeshLODs/LODSizeScale
	// can be connected at the render boundary.
	Data->MeshBuffer = StaticMesh->GetLODMeshBuffer(0);
	return Data;
}

bool FParticleMeshEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (!IsReplayType(OutData, EDynamicEmitterType::Mesh))
	{
		return false;
	}

	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return false;
	}

	FDynamicMeshEmitterReplayData& MeshData =
		static_cast<FDynamicMeshEmitterReplayData&>(OutData);

	MeshData.Material = GetCurrentMaterial();
	MeshData.SubUVInterpMethod = GetCurrentLODLevelChecked()->RequiredModule ? 0 : 0;
	MeshData.SubImages_Horizontal = GetCurrentLODLevelChecked()->RequiredModule ? GetCurrentLODLevelChecked()->RequiredModule->SubImages_Horizontal : 1;
	MeshData.SubImages_Vertical = GetCurrentLODLevelChecked()->RequiredModule ? GetCurrentLODLevelChecked()->RequiredModule->SubImages_Vertical : 1;
	MeshData.bScaleUV = false;
	MeshData.MeshRotationOffset = MeshRotationOffset;
	MeshData.MeshMotionBlurOffset = MeshMotionBlurOffset;
	MeshData.MeshAlignment = MeshTypeData ? static_cast<uint8>(MeshTypeData->MeshAlignment) : 0;
	MeshData.bMeshRotationActive = bMeshRotationActive;
	MeshData.Scale = FVector::OneVector;
	if (!GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace && !bIgnoreComponentScale)
	{
		MeshData.Scale = Component->GetWorldScale();
	}

	if (MeshTypeData && MeshTypeData->AxisLockOption != EPAL_NONE)
	{
		MeshData.bLockAxis = true;
		MeshData.LockedAxis = GetAxisLockVector(MeshTypeData->AxisLockOption);
	}
	else
	{
		MeshData.bLockAxis = false;
		MeshData.LockedAxis = FVector::ZAxisVector;
	}

	MeshData.SubUVDataOffset = SubUVDataOffset;
	MeshData.DynamicParameterDataOffset = DynamicParameterDataOffset;
	MeshData.LightDataOffset = LightDataOffset;
	MeshData.OrbitModuleOffset = OrbitModuleOffset;
	MeshData.CameraPayloadOffset = CameraPayloadOffset;
	MeshData.bUseLocalSpace = GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;

	MeshData.SectionMaterials.clear();
	MeshData.SectionFirstIndices.clear();
	MeshData.SectionIndexCounts.clear();
	if (UStaticMesh* StaticMesh = MeshTypeData ? MeshTypeData->GetStaticMesh() : nullptr)
	{
		const TArray<FStaticMeshSection>& Sections = StaticMesh->GetLODSections(0);
		TArray<UMaterial*> ResolvedMaterials;
		GetMeshMaterials(ResolvedMaterials, GetCurrentLODLevelChecked(), false);
		for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
		{
			UMaterial* SectionMaterial =
				SectionIdx < static_cast<int32>(ResolvedMaterials.size())
				? ResolvedMaterials[SectionIdx]
				: nullptr;
			MeshData.SectionMaterials.push_back(SectionMaterial);
			MeshData.SectionFirstIndices.push_back(Sections[SectionIdx].FirstIndex);
			MeshData.SectionIndexCounts.push_back(Sections[SectionIdx].NumTriangles * 3u);
		}
	}
	// UE original responsibility: choose static mesh LOD using bUseStaticMeshLODs and LODSizeScale.
	// Missing Jungle foundation: view-dependent mesh particle LOD size calculation.
	// System to connect later: StaticMesh LOD selector before FDynamicMeshEmitterData upload.

	return true;
}

namespace
{
	template <typename T>
	void EnsureIndexedValue(TArray<T>& Values, int32 Index, const T& DefaultValue)
	{
		if (Index < 0)
		{
			return;
		}
		if (static_cast<int32>(Values.size()) <= Index)
		{
			Values.resize(Index + 1, DefaultValue);
		}
	}

	FBeam2TypeDataPayload* GetActiveBeamPayloadByIndex(FParticleBeam2EmitterInstance& BeamInst, int32 BeamIndex)
	{
		if (BeamIndex < 0 || BeamIndex >= BeamInst.ActiveParticles || !BeamInst.BeamTypeData)
		{
			return nullptr;
		}

		FBaseParticle* Particle = BeamInst.GetParticle(BeamIndex);
		if (!Particle)
		{
			return nullptr;
		}

		int32 CurrentOffset = BeamInst.TypeDataOffset;
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
		BeamInst.BeamTypeData->GetDataPointers(&BeamInst, reinterpret_cast<const uint8*>(Particle), CurrentOffset,
			BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
			TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);
		return BeamData;
	}
}

void FParticleBeam2EmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent);
	bIsBeam = true;
	BeamTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataBeam2>(CurrentLODLevel->TypeDataModule) : nullptr;
}

void FParticleBeam2EmitterInstance::Init()
{
	FParticleEmitterInstance::Init();
	BeamTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataBeam2>(CurrentLODLevel->TypeDataModule) : BeamTypeData;
	FirstEmission = true;
	TickCount = 0;
	BeamCount = BeamTypeData ? std::max(1, BeamTypeData->MaxBeamCount) : 0;
	SetupBeamModifierModulesOffsets();
}

void FParticleBeam2EmitterInstance::SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess)
{
	FParticleEmitterInstance::SetCurrentLODIndex(InLODIndex, bInFullyProcess);
	BeamTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataBeam2>(CurrentLODLevel->TypeDataModule) : BeamTypeData;
	SetupBeamModifierModulesOffsets();
}

void FParticleBeam2EmitterInstance::SetupBeamModifierModulesOffsets()
{
	const int32 LODIndex = CurrentLODLevelIndex;
	if (BeamTypeData)
	{
		BeamModule_Source = LODIndex < static_cast<int32>(BeamTypeData->LOD_BeamModule_Source.size()) ? BeamTypeData->LOD_BeamModule_Source[LODIndex] : nullptr;
		BeamModule_Target = LODIndex < static_cast<int32>(BeamTypeData->LOD_BeamModule_Target.size()) ? BeamTypeData->LOD_BeamModule_Target[LODIndex] : nullptr;
		BeamModule_Noise = LODIndex < static_cast<int32>(BeamTypeData->LOD_BeamModule_Noise.size()) ? BeamTypeData->LOD_BeamModule_Noise[LODIndex] : nullptr;
		BeamModule_SourceModifier = LODIndex < static_cast<int32>(BeamTypeData->LOD_BeamModule_SourceModifier.size()) ? BeamTypeData->LOD_BeamModule_SourceModifier[LODIndex] : nullptr;
		BeamModule_TargetModifier = LODIndex < static_cast<int32>(BeamTypeData->LOD_BeamModule_TargetModifier.size()) ? BeamTypeData->LOD_BeamModule_TargetModifier[LODIndex] : nullptr;
		BeamMethod = BeamTypeData->BeamMethod;
	}
	BeamModule_SourceModifier_Offset = BeamModule_SourceModifier ? static_cast<int32>(GetModuleDataOffset(BeamModule_SourceModifier)) : INDEX_NONE;
	BeamModule_TargetModifier_Offset = BeamModule_TargetModifier ? static_cast<int32>(GetModuleDataOffset(BeamModule_TargetModifier)) : INDEX_NONE;
}

uint32 FParticleBeam2EmitterInstance::RequiredBytes()
{
	uint32 Bytes = FParticleEmitterInstance::RequiredBytes();
	if (BeamTypeData)
	{
		Bytes += BeamTypeData->RequiredBytes(BeamTypeData);
	}
	return Bytes;
}

void FParticleBeam2EmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (bEmitterIsDone)
	{
		return;
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);
	(void)EmitterDelay;
	if (bEnabled)
	{
		KillParticles();

		if (!bHaltSpawning && !bHaltSpawningExternal && !bSuppressSpawning && (EmitterTime >= 0.0f))
		{
			if ((LODLevel->RequiredModule->EmitterLoops == 0) ||
				(LoopCount < LODLevel->RequiredModule->EmitterLoops) ||
				(SecondsSinceCreation < (EmitterDuration * LODLevel->RequiredModule->EmitterLoops)))
			{
				float SpawnRate = 0.0f;
				if (LODLevel->SpawnModule)
				{
					SpawnRate = LODLevel->SpawnModule->SpawnRate * LODLevel->SpawnModule->SpawnRateScale;
				}

				int32 Burst = 0;
				float BurstTime = GetCurrentBurstRateOffset(DeltaTime, Burst);
				SpawnRate += BurstTime;

				const float InvDeltaTime = (DeltaTime > 0.0f) ? 1.0f / DeltaTime : 0.0f;
				if ((ActiveParticles < BeamCount) && (SpawnRate <= 0.0f))
				{
					SpawnRate = 1.0f * InvDeltaTime;
				}

				if ((ActiveParticles < BeamCount) && BeamTypeData && BeamTypeData->bAlwaysOn)
				{
					Burst = BeamCount;
					if (DeltaTime > 1.0e-4f)
					{
						BurstTime = Burst * InvDeltaTime;
						SpawnRate += BurstTime;
					}
				}

				if (SpawnRate > 0.0f)
				{
					SpawnFraction = SpawnBeamParticles(SpawnFraction, SpawnRate, DeltaTime, Burst, BurstTime);
				}
			}
		}
		else if (bFakeBurstsWhenSpawningSupressed)
		{
			FakeBursts();
		}

		ResetParticleParameters(DeltaTime);
		CurrentMaterial = LODLevel->RequiredModule->Material;
		Tick_ModuleUpdate(DeltaTime, LODLevel);
		Tick_ModulePostUpdate(DeltaTime, LODLevel);
		UpdateBoundingBox(DeltaTime);
		IsRenderDataDirty = 1;
		if (!bSuppressSpawning)
		{
			FirstEmission = false;
		}
		++TickCount;
	}
	else
	{
		FakeBursts();
	}
	EmitterTime += CurrentDelay;
	PositionOffsetThisTick = FVector::ZeroVector;
	LastDeltaTime = DeltaTime;
}

float FParticleBeam2EmitterInstance::SpawnBeamParticles(float OldLeftover, float Rate, float DeltaTime, int32 Burst, float BurstTime)
{
	if (!BeamTypeData)
	{
		return OldLeftover;
	}

	float SafetyLeftover = OldLeftover;
	float NewLeftover = OldLeftover + DeltaTime * Rate;

	int32 Number = static_cast<int32>(std::floor(NewLeftover));
	float Increment = (Rate > 0.0f) ? (1.0f / Rate) : 0.0f;
	float StartTime = DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover = NewLeftover - static_cast<float>(Number);

	if (Number < Burst)
	{
		Number = Burst;
	}

	if (BurstTime > 1.0e-4f && Burst > 0)
	{
		NewLeftover -= BurstTime / static_cast<float>(Burst);
		NewLeftover = std::max(0.0f, NewLeftover);
	}

	if (ActiveParticles == 0 && Number == 0)
	{
		Number = 1;
	}

	const int32 LocalBeamCount = std::max(1, BeamCount);
	if (Number + ActiveParticles > LocalBeamCount)
	{
		Number = LocalBeamCount - ActiveParticles;
	}

	bool bProcessSpawn = true;
	const int32 NewCount = ActiveParticles + Number;
	if (NewCount >= MaxActiveParticles)
	{
		const int32 Slack = static_cast<int32>(std::sqrt(static_cast<float>(std::max(NewCount, 1)))) + 1;
		if (DeltaTime < PeakActiveParticleUpdateDelta)
		{
			bProcessSpawn = Resize(NewCount + Slack);
		}
		else
		{
			bProcessSpawn = Resize(NewCount + Slack, false);
		}
	}

	if (bProcessSpawn)
	{
		SpawnParticles(Number, StartTime, Increment, Location, FVector::ZeroVector, nullptr);
		if (ForceSpawnCount > 0)
		{
			ForceSpawnCount = 0;
		}
		return NewLeftover;
	}

	return SafetyLeftover;
}

void FParticleBeam2EmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
	if (BeamModule_Source && BeamModule_Source->bEnabled)
	{
		BeamModule_Source->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Source)), SpawnTime, Particle });
	}
	if (BeamModule_SourceModifier && BeamModule_SourceModifier->bEnabled)
	{
		BeamModule_SourceModifier->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_SourceModifier)), SpawnTime, Particle });
	}
	if (BeamModule_Target && BeamModule_Target->bEnabled)
	{
		BeamModule_Target->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Target)), SpawnTime, Particle });
	}
	if (BeamModule_TargetModifier && BeamModule_TargetModifier->bEnabled)
	{
		BeamModule_TargetModifier->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_TargetModifier)), SpawnTime, Particle });
	}
	if (BeamModule_Noise && BeamModule_Noise->bEnabled)
	{
		BeamModule_Noise->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Noise)), SpawnTime, Particle });
	}
	if (BeamTypeData && BeamTypeData->bEnabled)
	{
		BeamTypeData->Spawn({ *this, TypeDataOffset, SpawnTime, Particle });
	}
	FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, SpawnTime);
}

void FParticleBeam2EmitterInstance::Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	if (BeamModule_Source && BeamModule_Source->bEnabled) BeamModule_Source->Update({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Source)), DeltaTime });
	if (BeamModule_SourceModifier && BeamModule_SourceModifier->bEnabled) BeamModule_SourceModifier->Update({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_SourceModifier)), DeltaTime });
	if (BeamModule_Target && BeamModule_Target->bEnabled) BeamModule_Target->Update({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Target)), DeltaTime });
	if (BeamModule_TargetModifier && BeamModule_TargetModifier->bEnabled) BeamModule_TargetModifier->Update({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_TargetModifier)), DeltaTime });
	if (BeamModule_Noise && BeamModule_Noise->bEnabled) BeamModule_Noise->Update({ *this, static_cast<int32>(GetModuleDataOffset(BeamModule_Noise)), DeltaTime });
	FParticleEmitterInstance::Tick_ModulePostUpdate(DeltaTime, CurrentLODLevel);
}

void FParticleBeam2EmitterInstance::ResolveSource()
{
	// UE original responsibility: resolve Actor/Emitter/Particle/Name lookup for beam sources.
	// Current support: Default source/target, UserSet source/target, Distance beam.
	// Current unsupported: Actor lookup, Emitter lookup, Particle lookup, Branch beam.
	// System to connect later: component instance parameters and emitter name lookup.
}

void FParticleBeam2EmitterInstance::ResolveTarget()
{
	// UE original responsibility: resolve Actor/Emitter/Particle/Name lookup for beam targets.
	// Current support: Default source/target, UserSet source/target, Distance beam.
	// Current unsupported: Actor lookup, Emitter lookup, Particle lookup, Branch beam.
	// System to connect later: component instance parameters and emitter name lookup.
}

void FParticleBeam2EmitterInstance::DetermineVertexAndTriangleCount()
{
	VertexCount = 0;
	TriangleCount = 0;
	BeamTrianglesPerSheet.clear();
	const int32 Sheets = BeamTypeData ? std::max(1, BeamTypeData->Sheets) : 1;
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		int32 CurrentOffset = TypeDataOffset;
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

		BeamTypeData->GetDataPointers(this, reinterpret_cast<const uint8*>(&Particle), CurrentOffset,
			BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
			TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);
		if (!BeamData)
		{
			BeamTrianglesPerSheet.push_back(0);
			continue;
		}

		BeamTrianglesPerSheet.push_back(BeamData->TriangleCount);
		if (BeamData->TriangleCount <= 0)
		{
			continue;
		}
		TriangleCount += BeamData->TriangleCount * Sheets;
		VertexCount += (BeamData->TriangleCount + 2) * Sheets;
	}
}

void FParticleBeam2EmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);

	// UE responsibility: beam bounds must include source / target endpoints and
	// low-frequency noise range. This stays in the emitter instance, not in
	// DynamicEmitterData.
	if (!BeamTypeData)
	{
		return;
	}

	for (int32 BeamIndex = 0; BeamIndex < ActiveParticles; ++BeamIndex)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[BeamIndex]);

		int32 CurrentOffset = TypeDataOffset;
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

		BeamTypeData->GetDataPointers(this, reinterpret_cast<const uint8*>(&Particle), CurrentOffset,
			BeamData, InterpolatedPoints, NoiseRate, NoiseDeltaTime, TargetNoisePoints, NextNoisePoints,
			TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

		if (!BeamData)
		{
			continue;
		}

		ParticleBoundingBox.Expand(BeamData->SourcePoint);
		ParticleBoundingBox.Expand(BeamData->TargetPoint);
		ParticleBoundingBox.Expand(Particle.Location);

		if (BeamModule_Noise)
		{
			FVector NoiseMin;
			FVector NoiseMax;
			BeamModule_Noise->GetNoiseRange(NoiseMin, NoiseMax);
			ParticleBoundingBox.Expand(BeamData->SourcePoint + NoiseMin);
			ParticleBoundingBox.Expand(BeamData->SourcePoint + NoiseMax);
			ParticleBoundingBox.Expand(BeamData->TargetPoint + NoiseMin);
			ParticleBoundingBox.Expand(BeamData->TargetPoint + NoiseMax);
		}
	}
}

void FParticleBeam2EmitterInstance::ForceUpdateBoundingBox()
{
	UpdateBoundingBox(0.0f);
}

void FParticleBeam2EmitterInstance::KillParticles()
{
	FParticleEmitterInstance::KillParticles();
}

void FParticleBeam2EmitterInstance::SetBeamEndPoint(FVector NewEndPoint) { SetBeamTargetPoint(NewEndPoint, 0); }
void FParticleBeam2EmitterInstance::SetBeamSourcePoint(FVector NewSourcePoint, int32 SourceIndex)
{
	EnsureIndexedValue(UserSetSourceArray, SourceIndex, FVector::ZeroVector);
	if (SourceIndex >= 0)
	{
		UserSetSourceArray[SourceIndex] = NewSourcePoint;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, SourceIndex))
		{
			BeamData->SourcePoint = NewSourcePoint;
		}
	}
}

void FParticleBeam2EmitterInstance::SetBeamSourceTangent(FVector NewTangentPoint, int32 SourceIndex)
{
	EnsureIndexedValue(UserSetSourceTangentArray, SourceIndex, FVector::ZeroVector);
	if (SourceIndex >= 0)
	{
		UserSetSourceTangentArray[SourceIndex] = NewTangentPoint;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, SourceIndex))
		{
			BeamData->SourceTangent = NewTangentPoint;
		}
	}
}

void FParticleBeam2EmitterInstance::SetBeamSourceStrength(float NewSourceStrength, int32 SourceIndex)
{
	EnsureIndexedValue(UserSetSourceStrengthArray, SourceIndex, 0.0f);
	if (SourceIndex >= 0)
	{
		UserSetSourceStrengthArray[SourceIndex] = NewSourceStrength;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, SourceIndex))
		{
			BeamData->SourceStrength = NewSourceStrength;
		}
	}
}

void FParticleBeam2EmitterInstance::SetBeamTargetPoint(FVector NewTargetPoint, int32 TargetIndex)
{
	EnsureIndexedValue(UserSetTargetArray, TargetIndex, FVector::ZeroVector);
	if (TargetIndex >= 0)
	{
		UserSetTargetArray[TargetIndex] = NewTargetPoint;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, TargetIndex))
		{
			BeamData->TargetPoint = NewTargetPoint;
		}
	}
}

void FParticleBeam2EmitterInstance::SetBeamTargetTangent(FVector NewTangentPoint, int32 TargetIndex)
{
	EnsureIndexedValue(UserSetTargetTangentArray, TargetIndex, FVector::ZeroVector);
	if (TargetIndex >= 0)
	{
		UserSetTargetTangentArray[TargetIndex] = NewTangentPoint;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, TargetIndex))
		{
			BeamData->TargetTangent = NewTangentPoint;
		}
	}
}

void FParticleBeam2EmitterInstance::SetBeamTargetStrength(float NewTargetStrength, int32 TargetIndex)
{
	EnsureIndexedValue(UserSetTargetStrengthArray, TargetIndex, 0.0f);
	if (TargetIndex >= 0)
	{
		UserSetTargetStrengthArray[TargetIndex] = NewTargetStrength;
		if (FBeam2TypeDataPayload* BeamData = GetActiveBeamPayloadByIndex(*this, TargetIndex))
		{
			BeamData->TargetStrength = NewTargetStrength;
		}
	}
}

bool FParticleBeam2EmitterInstance::GetBeamEndPoint(FVector& OutEndPoint) const { return GetBeamTargetPoint(0, OutEndPoint); }
bool FParticleBeam2EmitterInstance::GetBeamSourcePoint(int32 SourceIndex, FVector& OutSourcePoint) const { if (SourceIndex >= 0 && SourceIndex < static_cast<int32>(UserSetSourceArray.size())) { OutSourcePoint = UserSetSourceArray[SourceIndex]; return true; } return false; }
bool FParticleBeam2EmitterInstance::GetBeamSourceTangent(int32 SourceIndex, FVector& OutTangentPoint) const { if (SourceIndex >= 0 && SourceIndex < static_cast<int32>(UserSetSourceTangentArray.size())) { OutTangentPoint = UserSetSourceTangentArray[SourceIndex]; return true; } return false; }
bool FParticleBeam2EmitterInstance::GetBeamSourceStrength(int32 SourceIndex, float& OutSourceStrength) const { if (SourceIndex >= 0 && SourceIndex < static_cast<int32>(UserSetSourceStrengthArray.size())) { OutSourceStrength = UserSetSourceStrengthArray[SourceIndex]; return true; } return false; }
bool FParticleBeam2EmitterInstance::GetBeamTargetPoint(int32 TargetIndex, FVector& OutTargetPoint) const { if (TargetIndex >= 0 && TargetIndex < static_cast<int32>(UserSetTargetArray.size())) { OutTargetPoint = UserSetTargetArray[TargetIndex]; return true; } return false; }
bool FParticleBeam2EmitterInstance::GetBeamTargetTangent(int32 TargetIndex, FVector& OutTangentPoint) const { if (TargetIndex >= 0 && TargetIndex < static_cast<int32>(UserSetTargetTangentArray.size())) { OutTangentPoint = UserSetTargetTangentArray[TargetIndex]; return true; } return false; }
bool FParticleBeam2EmitterInstance::GetBeamTargetStrength(int32 TargetIndex, float& OutTargetStrength) const { if (TargetIndex >= 0 && TargetIndex < static_cast<int32>(UserSetTargetStrengthArray.size())) { OutTargetStrength = UserSetTargetStrengthArray[TargetIndex]; return true; } return false; }

void FParticleBeam2EmitterInstance::ApplyWorldOffset(FVector InOffset, bool bWorldShift)
{
	FParticleEmitterInstance::ApplyWorldOffset(InOffset, bWorldShift);
	for (FVector& Source : UserSetSourceArray) Source += InOffset;
	for (FVector& Target : UserSetTargetArray) Target += InOffset;
}

UMaterial* FParticleBeam2EmitterInstance::GetCurrentMaterial()
{
	return FParticleEmitterInstance::GetCurrentMaterial();
}

FDynamicEmitterDataBase* FParticleBeam2EmitterInstance::GetDynamicData(bool bSelected)
{
	FDynamicBeam2EmitterData* Data = new FDynamicBeam2EmitterData();
	if (!FillReplayData(Data->Source))
	{
		delete Data;
		return nullptr;
	}
	return Data;
}

bool FParticleBeam2EmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (!IsReplayType(OutData, EDynamicEmitterType::Beam)) return false;
	if (!FParticleEmitterInstance::FillReplayData(OutData)) return false;

	FDynamicBeam2EmitterReplayData& BeamData = static_cast<FDynamicBeam2EmitterReplayData&>(OutData);
	DetermineVertexAndTriangleCount();

	BeamData.Material = GetCurrentMaterial();
	BeamData.VertexCount = VertexCount;
	BeamData.TrianglesPerSheet = BeamTrianglesPerSheet;
	BeamData.Sheets = BeamTypeData ? std::max(1, BeamTypeData->Sheets) : 1;
	BeamData.InterpolationPoints = BeamTypeData ? BeamTypeData->InterpolationPoints : 0;
	BeamData.TextureTile = BeamTypeData ? BeamTypeData->TextureTile : 1;
	BeamData.TextureTileDistance = BeamTypeData ? BeamTypeData->TextureTileDistance : 0.0f;
	BeamData.TaperMethod = BeamTypeData ? static_cast<uint8>(BeamTypeData->TaperMethod) : 0;
	BeamData.UpVectorStepSize = BeamTypeData ? BeamTypeData->UpVectorStepSize : 0;
	BeamData.bRenderGeometry = BeamTypeData ? BeamTypeData->RenderGeometry : true;
	BeamData.bRenderDirectLine = BeamTypeData ? BeamTypeData->RenderDirectLine : false;
	BeamData.bRenderLines = BeamTypeData ? BeamTypeData->RenderLines : false;
	BeamData.bRenderTessellation = BeamTypeData ? BeamTypeData->RenderTessellation : false;

	if (BeamTypeData)
	{
		int32 TypeDataCurrentOffset = TypeDataOffset;
		int32 TaperCount = 0;
		BeamTypeData->GetDataPointerOffsets(this, nullptr, TypeDataCurrentOffset,
			BeamData.BeamDataOffset,
			BeamData.InterpolatedPointsOffset,
			BeamData.NoiseRateOffset,
			BeamData.NoiseDeltaTimeOffset,
			BeamData.TargetNoisePointsOffset,
			BeamData.NextNoisePointsOffset,
			TaperCount,
			BeamData.TaperValuesOffset,
			BeamData.NoiseDistanceScaleOffset);
	}
	else
	{
		BeamData.BeamDataOffset = TypeDataOffset;
	}

	BeamData.bUseSource = BeamModule_Source != nullptr;
	BeamData.bUseTarget = BeamModule_Target != nullptr;
	BeamData.bLowFreqNoise_Enabled = BeamModule_Noise ? BeamModule_Noise->bLowFreq_Enabled : false;
	BeamData.bHighFreqNoise_Enabled = false;
	BeamData.bSmoothNoise_Enabled = BeamModule_Noise ? BeamModule_Noise->bSmooth : false;
	BeamData.bTargetNoise = BeamModule_Noise ? BeamModule_Noise->bTargetNoise : false;
	BeamData.Frequency = BeamModule_Noise ? std::max(1, BeamModule_Noise->Frequency) : 1;
	BeamData.NoiseTessellation = BeamModule_Noise ? std::max(1, BeamModule_Noise->NoiseTessellation) : 0;
	BeamData.NoiseTangentStrength = BeamModule_Noise ? BeamModule_Noise->NoiseTangentStrength.GetValue(EmitterTime, Component) : 1.0f;
	BeamData.NoiseRangeScale = BeamModule_Noise ? BeamModule_Noise->NoiseRangeScale.GetValue(EmitterTime, Component) : 1.0f;
	BeamData.NoiseSpeed = BeamModule_Noise ? BeamModule_Noise->NoiseSpeed.GetValue(EmitterTime, Component) : FVector::ZeroVector;
	BeamData.NoiseLockTime = BeamModule_Noise ? BeamModule_Noise->NoiseLockTime : 0.0f;
	BeamData.NoiseLockRadius = BeamModule_Noise ? BeamModule_Noise->NoiseLockRadius : 0.0f;
	BeamData.NoiseTension = BeamModule_Noise ? BeamModule_Noise->NoiseTension : 0.0f;

	// UE source stores beam/trail as strips with degenerate joins. Jungle's CPU
	// adapter keeps the UE logical point/sheet generation, then converts only the
	// final emitted index stream to triangle-list indices.
	BeamData.IndexCount = TriangleCount * 3;
	BeamData.IndexStride = (BeamData.IndexCount > 15000) ? sizeof(uint32) : sizeof(uint16);
	return true;
}

FParticleTrailsEmitterInstance_Base::FParticleTrailsEmitterInstance_Base()
	: bDeadTrailsOnDeactivate(false)
	, bFirstUpdate(true)
	, bEnableInactiveTimeTracking(false)
{
	for (int32 TrailIdx = 0; TrailIdx < 128; ++TrailIdx)
	{
		CurrentStartIndices[TrailIdx] = INDEX_NONE;
		CurrentEndIndices[TrailIdx] = INDEX_NONE;
	}
}

void FParticleTrailsEmitterInstance_Base::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent);
}

void FParticleTrailsEmitterInstance_Base::Init()
{
	FParticleEmitterInstance::Init();
	RunningTime = 0.0f;
	LastTickTime = 0.0f;
	bFirstUpdate = true;
}

void FParticleTrailsEmitterInstance_Base::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (bEmitterIsDone)
	{
		return;
	}

	const bool bFirstTime = SecondsSinceCreation <= 0.0f;
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);
	(void)EmitterDelay;

	UpdateSourceData(DeltaTime, bFirstTime);
	KillParticles();
	SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSuppressSpawning, bFirstTime);
	ResetParticleParameters(DeltaTime);
	Tick_ModuleUpdate(DeltaTime, LODLevel);
	Tick_ModulePostUpdate(DeltaTime, LODLevel);
	UpdateBoundingBox(DeltaTime);
	Tick_ModuleFinalUpdate(DeltaTime, LODLevel);
	Tick_RecalculateTangents(DeltaTime, LODLevel);

	CurrentMaterial = LODLevel->RequiredModule->Material;
	IsRenderDataDirty = 1;
	EmitterTime += CurrentDelay;
	RunningTime += DeltaTime;
	LastTickTime = Component ? Component->GetWorldTimeSeconds() : SecondsSinceCreation;
	LastDeltaTime = DeltaTime;
	PositionOffsetThisTick = FVector::ZeroVector;
}

bool FParticleTrailsEmitterInstance_Base::AddParticleHelper(int32 InTrailIdx, int32 StartParticleIndex, FTrailsBaseTypeDataPayload* StartTrailData, int32 ParticleIndex, FTrailsBaseTypeDataPayload* TrailData)
{
	bool bAddedParticle = false;

	TrailData->TrailIndex = InTrailIdx;

	if (TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags))
	{
		StartTrailData->Flags = TRAIL_EMITTER_SET_END(StartTrailData->Flags);
		StartTrailData->Flags = TRAIL_EMITTER_SET_NEXT(StartTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
		StartTrailData->Flags = TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);
		SetEndIndex(StartTrailData->TrailIndex, StartParticleIndex);
	}
	else
	{
		if (!TRAIL_EMITTER_IS_START(StartTrailData->Flags) ||
			TRAIL_EMITTER_GET_NEXT(StartTrailData->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			return false;
		}
		StartTrailData->Flags = TRAIL_EMITTER_SET_MIDDLE(StartTrailData->Flags);
		StartTrailData->Flags = TRAIL_EMITTER_SET_PREV(StartTrailData->Flags, ParticleIndex);
		ClearIndices(StartTrailData->TrailIndex, StartParticleIndex);
	}

	TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
	TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, StartParticleIndex);
	TrailData->Flags = TRAIL_EMITTER_SET_START(TrailData->Flags);

	SetStartIndex(TrailData->TrailIndex, ParticleIndex);
	bAddedParticle = true;

	return bAddedParticle;
}

void FParticleTrailsEmitterInstance_Base::Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	(void)DeltaTime;
	(void)CurrentLODLevel;
}

void FParticleTrailsEmitterInstance_Base::UpdateBoundingBox(float DeltaTime)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	const bool bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = bUseLocalSpace ? Component->GetWorldMatrix() : FMatrix::Identity;
	const FVector Scale = Component->GetWorldScale();

	ParticleBoundingBox = FBoundingBox();
	if (!bUseLocalSpace)
	{
		ParticleBoundingBox.Expand(Component->GetWorldLocation());
	}
	else
	{
		ParticleBoundingBox.Expand(FVector::ZeroVector);
	}

	const bool bSkipDoubleSpawnUpdate = SpriteTemplate ? !SpriteTemplate->bUseLegacySpawningBehavior : true;
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
		const FVector Size = Particle.Size * Scale;
		const bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		Particle.Flags &= ~STATE_Particle_JustSpawned;
		const bool bSkipUpdate = bJustSpawned && bSkipDoubleSpawnUpdate;

		if (!bSkipUpdate)
		{
			Particle.Location += Particle.Velocity * DeltaTime;
			Particle.Rotation += Particle.RotationRate * DeltaTime;
		}
		Particle.Location += PositionOffsetThisTick;
		Particle.OldLocation = Particle.Location;
		Particle.Rotation = std::fmod(Particle.Rotation, 2.0f * FMath::Pi);

		FVector PositionForBounds = Particle.Location;
		if (bUseLocalSpace)
		{
			PositionForBounds = ComponentToWorld.TransformPosition(Particle.Location);
		}
		const FVector AbsSize(std::fabs(Size.X), std::fabs(Size.Y), std::fabs(Size.Z));
		ParticleBoundingBox.Expand(PositionForBounds - AbsSize);
		ParticleBoundingBox.Expand(PositionForBounds + AbsSize);
	}
}

void FParticleTrailsEmitterInstance_Base::ForceUpdateBoundingBox()
{
	UpdateBoundingBox(0.0f);
}

void FParticleTrailsEmitterInstance_Base::KillParticles()
{
	if (ActiveParticles <= 0 || !ParticleData || !ParticleIndices)
	{
		return;
	}

	bool bHasForceKillParticles = false;
	for (int32 ParticleIdx = ActiveParticles - 1; ParticleIdx >= 0; --ParticleIdx)
	{
		const int32 CurrentIndex = ParticleIndices[ParticleIdx];
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * CurrentIndex);
		FTrailsBaseTypeDataPayload* TrailData =
			reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(&Particle) + TypeDataOffset);

		const bool bInactiveTimedOut =
			bEnableInactiveTimeTracking &&
			(Component ? Component->GetWorldTimeSeconds() : SecondsSinceCreation) != 0.0f &&
			(Particle.OneOverMaxLifetime > 0.0f) &&
			(((Component ? Component->GetWorldTimeSeconds() : SecondsSinceCreation) - LastTickTime) > (1.0f / Particle.OneOverMaxLifetime));

		if (Particle.RelativeTime <= 1.0f && !bInactiveTimedOut && !TRAIL_EMITTER_IS_FORCEKILL(TrailData->Flags))
		{
			continue;
		}

		if (TRAIL_EMITTER_IS_HEAD(TrailData->Flags) || TRAIL_EMITTER_IS_ONLY(TrailData->Flags))
		{
			const int32 Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
			if (Next != TRAIL_EMITTER_NULL_NEXT)
			{
				FBaseParticle* NextParticle = GetParticleDirect(Next);
				FTrailsBaseTypeDataPayload* NextTrailData = NextParticle
					? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(NextParticle) + TypeDataOffset)
					: nullptr;
				if (NextTrailData)
				{
					if (TRAIL_EMITTER_IS_END(NextTrailData->Flags))
					{
						if (TRAIL_EMITTER_IS_START(TrailData->Flags))
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_ONLY(NextTrailData->Flags);
							SetStartIndex(NextTrailData->TrailIndex, Next);
							SetEndIndex(NextTrailData->TrailIndex, Next);
						}
						else if (TRAIL_EMITTER_IS_DEADTRAIL(TrailData->Flags))
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(NextTrailData->Flags);
							SetDeadIndex(NextTrailData->TrailIndex, Next);
						}
					}
					else
					{
						if (TRAIL_EMITTER_IS_START(TrailData->Flags))
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_START(NextTrailData->Flags);
							SetStartIndex(NextTrailData->TrailIndex, Next);
						}
						else
						{
							NextTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(NextTrailData->Flags);
							SetDeadIndex(NextTrailData->TrailIndex, Next);
						}
					}
					NextTrailData->Flags = TRAIL_EMITTER_SET_PREV(NextTrailData->Flags, TRAIL_EMITTER_NULL_PREV);
				}
			}
		}
		else if (TRAIL_EMITTER_IS_END(TrailData->Flags))
		{
			const int32 Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				FBaseParticle* PrevParticle = GetParticleDirect(Prev);
				FTrailsBaseTypeDataPayload* PrevTrailData = PrevParticle
					? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(PrevParticle) + TypeDataOffset)
					: nullptr;
				if (PrevTrailData)
				{
					if (TRAIL_EMITTER_IS_START(PrevTrailData->Flags))
					{
						PrevTrailData->Flags = TRAIL_EMITTER_SET_ONLY(PrevTrailData->Flags);
						SetStartIndex(PrevTrailData->TrailIndex, Prev);
						SetEndIndex(PrevTrailData->TrailIndex, Prev);
					}
					else if (TRAIL_EMITTER_IS_DEADTRAIL(PrevTrailData->Flags))
					{
						PrevTrailData->TriangleCount = 0;
						PrevTrailData->RenderingInterpCount = 1;
					}
					else
					{
						PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						SetEndIndex(PrevTrailData->TrailIndex, Prev);
					}
					PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				}
			}
		}
		else if (TRAIL_EMITTER_IS_MIDDLE(TrailData->Flags))
		{
			const int32 Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
			int32 Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				FBaseParticle* PrevParticle = GetParticleDirect(Prev);
				FTrailsBaseTypeDataPayload* PrevTrailData = PrevParticle
					? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(PrevParticle) + TypeDataOffset)
					: nullptr;
				if (PrevTrailData)
				{
					if (TRAIL_EMITTER_IS_START(PrevTrailData->Flags))
					{
						PrevTrailData->Flags = TRAIL_EMITTER_SET_ONLY(PrevTrailData->Flags);
						SetStartIndex(PrevTrailData->TrailIndex, Prev);
						SetEndIndex(PrevTrailData->TrailIndex, Prev);
					}
					else if (TRAIL_EMITTER_IS_DEADTRAIL(PrevTrailData->Flags))
					{
						PrevTrailData->TriangleCount = 0;
						PrevTrailData->RenderingInterpCount = 1;
					}
					else
					{
						PrevTrailData->Flags = TRAIL_EMITTER_SET_END(PrevTrailData->Flags);
						SetEndIndex(PrevTrailData->TrailIndex, Prev);
					}
					PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				}
			}

			while (Next != TRAIL_EMITTER_NULL_NEXT)
			{
				FBaseParticle* NextParticle = GetParticleDirect(Next);
				FTrailsBaseTypeDataPayload* NextTrailData = NextParticle
					? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(NextParticle) + TypeDataOffset)
					: nullptr;
				if (!NextTrailData)
				{
					break;
				}
				NextTrailData->Flags = TRAIL_EMITTER_SET_FORCEKILL(NextTrailData->Flags);
				SetDeadIndex(NextTrailData->TrailIndex, Next);
				Next = TRAIL_EMITTER_GET_NEXT(NextTrailData->Flags);
				bHasForceKillParticles = true;
			}
		}

		if (!TRAIL_EMITTER_IS_FORCEKILL(TrailData->Flags))
		{
			const int32 Next = TRAIL_EMITTER_GET_NEXT(TrailData->Flags);
			if (Next != TRAIL_EMITTER_NULL_NEXT)
			{
				if (FBaseParticle* NextParticle = GetParticleDirect(Next))
				{
					FTrailsBaseTypeDataPayload* NextTrailData =
						reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(NextParticle) + TypeDataOffset);
					NextTrailData->Flags = TRAIL_EMITTER_SET_PREV(NextTrailData->Flags, TRAIL_EMITTER_NULL_PREV);
				}
			}
			const int32 Prev = TRAIL_EMITTER_GET_PREV(TrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				if (FBaseParticle* PrevParticle = GetParticleDirect(Prev))
				{
					FTrailsBaseTypeDataPayload* PrevTrailData =
						reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(PrevParticle) + TypeDataOffset);
					PrevTrailData->Flags = TRAIL_EMITTER_SET_NEXT(PrevTrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
				}
			}
		}

		TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
		TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
		ParticleIndices[ParticleIdx] = ParticleIndices[ActiveParticles - 1];
		ParticleIndices[ActiveParticles - 1] = static_cast<uint16>(CurrentIndex);
		--ActiveParticles;
		SetDeadIndex(TrailData->TrailIndex, CurrentIndex);
	}

	if (bHasForceKillParticles)
	{
		KillParticles();
	}
}

void FParticleTrailsEmitterInstance_Base::KillParticles(int32 InTrailIdx, int32 InKillCount)
{
	if (ActiveParticles <= 0)
	{
		return;
	}

	int32 KilledCount = 0;
	int32 EndIndex = INDEX_NONE;
	FTrailsBaseTypeDataPayload* EndTrailData = nullptr;
	FBaseParticle* EndParticle = nullptr;
	GetTrailEnd<FTrailsBaseTypeDataPayload>(InTrailIdx, EndIndex, EndTrailData, EndParticle);
	if (EndParticle && EndTrailData && EndTrailData->TrailIndex == InTrailIdx)
	{
		while (EndTrailData && KilledCount < InKillCount)
		{
			EndParticle->RelativeTime = 1.1f;
			++KilledCount;
			const int32 Prev = TRAIL_EMITTER_GET_PREV(EndTrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				EndParticle = GetParticleDirect(Prev);
				EndTrailData = EndParticle
					? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(EndParticle) + TypeDataOffset)
					: nullptr;
				if (EndTrailData && TRAIL_EMITTER_IS_START(EndTrailData->Flags))
				{
					EndTrailData = nullptr;
				}
				else if (EndTrailData && TRAIL_EMITTER_IS_DEADTRAIL(EndTrailData->Flags))
				{
					EndTrailData->TriangleCount = 0;
					EndTrailData->RenderingInterpCount = 1;
				}
			}
			else
			{
				EndTrailData = nullptr;
			}
		}
	}

	if (KilledCount > 0)
	{
		KillParticles();
	}
}

void FParticleTrailsEmitterInstance_Base::UpdateSourceData(float DeltaTime, bool bFirstTime)
{
	(void)DeltaTime;
	(void)bFirstTime;
}

void FParticleTrailsEmitterInstance_Base::SetStartIndex(int32 TrailIndex, int32 ParticleIndex)
{
	if (TrailIndex >= 0 && TrailIndex < 128)
	{
		CurrentStartIndices[TrailIndex] = ParticleIndex;
	}
}

void FParticleTrailsEmitterInstance_Base::SetEndIndex(int32 TrailIndex, int32 ParticleIndex)
{
	if (TrailIndex >= 0 && TrailIndex < 128)
	{
		CurrentEndIndices[TrailIndex] = ParticleIndex;
	}
}

void FParticleTrailsEmitterInstance_Base::SetDeadIndex(int32 TrailIndex, int32 ParticleIndex)
{
	if (TrailIndex >= 0 && TrailIndex < 128)
	{
		if (CurrentStartIndices[TrailIndex] == ParticleIndex) CurrentStartIndices[TrailIndex] = INDEX_NONE;
		if (CurrentEndIndices[TrailIndex] == ParticleIndex) CurrentEndIndices[TrailIndex] = INDEX_NONE;
	}
}

void FParticleTrailsEmitterInstance_Base::ClearIndices(int32 TrailIndex, int32 ParticleIndex)
{
	if (TrailIndex >= 0 && TrailIndex < 128)
	{
		if (CurrentStartIndices[TrailIndex] == ParticleIndex)
		{
			CurrentStartIndices[TrailIndex] = INDEX_NONE;
		}
		if (CurrentEndIndices[TrailIndex] == ParticleIndex)
		{
			CurrentEndIndices[TrailIndex] = INDEX_NONE;
		}
	}
}

bool FParticleTrailsEmitterInstance_Base::GetParticleInTrail(bool bSkipStartingParticle, FBaseParticle* InStartingFromParticle, FTrailsBaseTypeDataPayload* InStartingTrailData, EGetTrailDirection InGetDirection, EGetTrailParticleOption InGetOption, FBaseParticle*& OutParticle, FTrailsBaseTypeDataPayload*& OutTrailData)
{
	OutParticle = nullptr;
	OutTrailData = nullptr;
	if (!InStartingFromParticle || !InStartingTrailData)
	{
		return false;
	}

	bool bDone = false;
	FBaseParticle* CheckParticle = InStartingFromParticle;
	FTrailsBaseTypeDataPayload* CheckTrailData = InStartingTrailData;
	bool bCheckIt = !bSkipStartingParticle;
	while (!bDone)
	{
		if (bCheckIt)
		{
			bool bItsGood = false;
			switch (InGetOption)
			{
			case GET_Any:
				bItsGood = true;
				break;
			case GET_Spawned:
				bItsGood = !CheckTrailData->bInterpolatedSpawn;
				break;
			case GET_Interpolated:
				bItsGood = CheckTrailData->bInterpolatedSpawn != 0;
				break;
			case GET_Start:
				bItsGood = TRAIL_EMITTER_IS_START(CheckTrailData->Flags);
				break;
			case GET_End:
				bItsGood = TRAIL_EMITTER_IS_END(CheckTrailData->Flags);
				break;
			default:
				break;
			}

			if (bItsGood)
			{
				OutParticle = CheckParticle;
				OutTrailData = CheckTrailData;
				bDone = true;
			}
		}

		int32 Index = INDEX_NONE;
		if (!bDone)
		{
			if (InGetDirection == GET_Prev)
			{
				Index = TRAIL_EMITTER_GET_PREV(CheckTrailData->Flags);
				if (Index == TRAIL_EMITTER_NULL_PREV)
				{
					Index = INDEX_NONE;
				}
			}
			else
			{
				Index = TRAIL_EMITTER_GET_NEXT(CheckTrailData->Flags);
				if (Index == TRAIL_EMITTER_NULL_NEXT)
				{
					Index = INDEX_NONE;
				}
			}
		}

		if (Index != INDEX_NONE)
		{
			CheckParticle = GetParticleDirect(Index);
			CheckTrailData = CheckParticle
				? reinterpret_cast<FTrailsBaseTypeDataPayload*>(reinterpret_cast<uint8*>(CheckParticle) + TypeDataOffset)
				: nullptr;
			bCheckIt = true;
			if (!CheckParticle || !CheckTrailData)
			{
				bDone = true;
			}
		}
		else
		{
			bDone = true;
		}
	}

	return OutParticle && OutTrailData;
}

void FParticleRibbonEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleTrailsEmitterInstance_Base::InitParameters(InTemplate, InComponent);
	TrailTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->TypeDataModule) : nullptr;
	SetupTrailModules();
	const int32 Count = TrailTypeData ? std::max(1, TrailTypeData->MaxTrailCount) : 1;
	MaxTrailCount = Count;
	CurrentSourcePosition.resize(Count, Location);
	LastSourcePosition.resize(Count, Location);
	CurrentSourceRotation.resize(Count, FQuat::Identity);
	LastSourceRotation.resize(Count, FQuat::Identity);
	CurrentSourceUp.resize(Count, FVector::ZAxisVector);
	LastSourceUp.resize(Count, FVector::ZAxisVector);
	CurrentSourceTangent.resize(Count, FVector::XAxisVector);
	LastSourceTangent.resize(Count, FVector::XAxisVector);
	CurrentSourceTangentStrength.resize(Count, 0.0f);
	LastSourceTangentStrength.resize(Count, 0.0f);
	SourceDistanceTraveled.resize(Count, 0.0f);
	TiledUDistanceTraveled.resize(Count, 0.0f);
	TrailSpawnTimes.resize(Count, 0.0f);
	LastSpawnTime.resize(Count, 0.0f);
	SourceIndices.resize(Count, INDEX_NONE);
	SourceTimes.resize(Count, 0.0f);
	LastSourceTimes.resize(Count, 0.0f);
	CurrentLifetimes.resize(Count, 0.0f);
	CurrentSizes.resize(Count, 0.0f);
}

void FParticleRibbonEmitterInstance::SetupTrailModules()
{
	SpawnPerUnitModule = nullptr;
	SourceModule = nullptr;
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	for (UParticleModule* Module : LODLevel->Modules)
	{
		if (!SpawnPerUnitModule) SpawnPerUnitModule = Cast<UParticleModuleSpawnPerUnit>(Module);
		if (!SourceModule) SourceModule = Cast<UParticleModuleTrailSource>(Module);
	}
	TrailModule_Source_Offset = SourceModule ? static_cast<int32>(GetModuleDataOffset(SourceModule)) : INDEX_NONE;

	auto RemoveTrailModule = [](TArray<UParticleModule*>& Modules, UParticleModule* Module)
	{
		if (!Module)
		{
			return;
		}
		Modules.erase(std::remove(Modules.begin(), Modules.end(), Module), Modules.end());
	};

	// UE original responsibility: trail source and spawn-per-unit modules are
	// cached for the trail instance and removed from generic Spawn/Update lists.
	RemoveTrailModule(LODLevel->SpawnModules, SpawnPerUnitModule);
	RemoveTrailModule(LODLevel->UpdateModules, SpawnPerUnitModule);
	RemoveTrailModule(LODLevel->SpawnModules, SourceModule);
	RemoveTrailModule(LODLevel->UpdateModules, SourceModule);
}

float FParticleRibbonEmitterInstance::Spawn(float DeltaTime)
{
	const bool bProcessSpawnRate = Spawn_Source(DeltaTime);
	return bProcessSpawnRate ? Spawn_RateAndBurst(DeltaTime) : SpawnFraction;
}

bool FParticleRibbonEmitterInstance::Spawn_Source(float DeltaTime)
{
	bool bProcessSpawnRate = true;
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	const int32 LocalMaxParticleInTrailCount = TrailTypeData ? TrailTypeData->MaxParticleInTrailCount : 0;

	for (int32 TrailIdx = 0; TrailIdx < MaxTrailCount; ++TrailIdx)
	{
		int32 MovementSpawnCount = 0;
		float MovementSpawnRate = 0.0f;
		if (SpawnPerUnitModule && SpawnPerUnitModule->bEnabled)
		{
			bProcessSpawnRate = GetSpawnPerUnitAmount(DeltaTime, TrailIdx, MovementSpawnCount, MovementSpawnRate);
		}

		int32 StartIndex = INDEX_NONE;
		FRibbonTypeDataPayload* StartTrailData = nullptr;
		FBaseParticle* StartParticle = nullptr;
		for (int32 FindTrailIdx = 0; FindTrailIdx < ActiveParticles; ++FindTrailIdx)
		{
			const int32 CheckStartIndex = ParticleIndices[FindTrailIdx];
			FBaseParticle* CheckParticle = GetParticleDirect(CheckStartIndex);
			FRibbonTypeDataPayload* CheckTrailData = CheckParticle ? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(CheckParticle) + TypeDataOffset) : nullptr;
			if (CheckTrailData && CheckTrailData->TrailIndex == TrailIdx && TRAIL_EMITTER_IS_START(CheckTrailData->Flags))
			{
				StartParticle = CheckParticle;
				StartIndex = CheckStartIndex;
				StartTrailData = CheckTrailData;
				break;
			}
		}

		if (TrailTypeData && TrailTypeData->bDeadTrailsOnSourceLoss && LastSourceTimes[TrailIdx] > SourceTimes[TrailIdx])
		{
			if (StartTrailData)
			{
				StartTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(StartTrailData->Flags);
				SetDeadIndex(StartTrailData->TrailIndex, StartIndex);
			}
			StartParticle = nullptr;
			StartIndex = INDEX_NONE;
			StartTrailData = nullptr;
			LastSourcePosition[TrailIdx] = CurrentSourcePosition[TrailIdx];
			LastSourceRotation[TrailIdx] = CurrentSourceRotation[TrailIdx];
			LastSourceTangent[TrailIdx] = CurrentSourceTangent[TrailIdx];
			LastSourceUp[TrailIdx] = CurrentSourceUp[TrailIdx];
			LastSourceTimes[TrailIdx] = SourceTimes[TrailIdx];
			MovementSpawnCount = 0;
			SourceIndices[TrailIdx] = INDEX_NONE;
			continue;
		}

		if (LocalMaxParticleInTrailCount > 0)
		{
			int32 KillCount = (MovementSpawnCount + ActiveParticles) - LocalMaxParticleInTrailCount;
			if (KillCount > 0)
			{
				KillParticles(TrailIdx, KillCount);
				GetTrailStart<FRibbonTypeDataPayload>(TrailIdx, StartIndex, StartTrailData, StartParticle);
			}
			if ((MovementSpawnCount + ActiveParticles) > LocalMaxParticleInTrailCount)
			{
				MovementSpawnCount = std::max(0, LocalMaxParticleInTrailCount - ActiveParticles);
			}
		}

		if (MovementSpawnCount <= 0)
		{
			continue;
		}

		const int32 NewCount = ActiveParticles + MovementSpawnCount;
		if (NewCount >= MaxActiveParticles)
		{
			const int32 Slack = static_cast<int32>(std::sqrt(static_cast<float>(std::max(NewCount, 1)))) + 1;
			Resize(NewCount + Slack, DeltaTime < PeakActiveParticleUpdateDelta);
		}

		bool bNoLivingParticles = StartParticle == nullptr;
		const bool bTilingTrail = TrailTypeData && std::fabs(TrailTypeData->TilingDistance) >= 1.0e-6f;
		const float ElapsedTime = RunningTime;
		if (SecondsSinceCreation < TrailSpawnTimes[TrailIdx] && ElapsedTime > 1.0e-6f)
		{
			LastSourceTangent[TrailIdx] = (CurrentSourcePosition[TrailIdx] - LastSourcePosition[TrailIdx]) / ElapsedTime;
		}

		const float LastTime = TrailSpawnTimes[TrailIdx];
		const float Diff = std::max(0.0f, ElapsedTime - LastTime);
		const float InvCount = MovementSpawnCount > 0 ? 1.0f / static_cast<float>(MovementSpawnCount) : 0.0f;
		const float Increment = MovementSpawnCount > 0 ? DeltaTime / static_cast<float>(MovementSpawnCount) : 0.0f;

		if (TrailTypeData && TrailTypeData->bEnablePreviousTangentRecalculation && !TrailTypeData->bTangentRecalculationEveryFrame && StartParticle && StartTrailData)
		{
			FBaseParticle* NextSpawnedParticle = nullptr;
			FTrailsBaseTypeDataPayload* TempPayload = nullptr;
			GetParticleInTrail(false, StartParticle, StartTrailData, GET_Next, GET_Spawned, NextSpawnedParticle, TempPayload);
			FRibbonTypeDataPayload* NextSpawnedTrailData = static_cast<FRibbonTypeDataPayload*>(TempPayload);

			FBaseParticle* NextNextSpawnedParticle = nullptr;
			TempPayload = nullptr;
			GetParticleInTrail(true, NextSpawnedParticle, NextSpawnedTrailData, GET_Next, GET_Spawned, NextNextSpawnedParticle, TempPayload);
			FRibbonTypeDataPayload* NextNextSpawnedTrailData = static_cast<FRibbonTypeDataPayload*>(TempPayload);

			if (NextSpawnedParticle && NextSpawnedTrailData && NextNextSpawnedParticle && NextNextSpawnedTrailData)
			{
				const FVector PositionDelta = CurrentSourcePosition[TrailIdx] - PositionOffsetThisTick - NextNextSpawnedParticle->Location;
				const float TimeDelta = ElapsedTime - NextNextSpawnedTrailData->SpawnTime;
				FVector NewTangent = FVector::ZeroVector;
				if (TimeDelta > 1.0e-6f)
				{
					NewTangent = PositionDelta / TimeDelta;
				}

				if (NextSpawnedTrailData->SpawnedTessellationPoints > 0)
				{
					const int32 Prev = TRAIL_EMITTER_GET_PREV(NextNextSpawnedTrailData->Flags);
					FBaseParticle* CurrentParticle = (Prev != TRAIL_EMITTER_NULL_PREV) ? GetParticleDirect(Prev) : nullptr;
					FRibbonTypeDataPayload* CurrentTrailData = CurrentParticle ? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(CurrentParticle) + TypeDataOffset) : nullptr;
					const float SpawnDiff = NextSpawnedTrailData->SpawnTime - NextNextSpawnedTrailData->SpawnTime;
					const float SpawnInvCount = 1.0f / static_cast<float>(NextSpawnedTrailData->SpawnedTessellationPoints);

					for (int32 RecalcIdx = 0; RecalcIdx < NextSpawnedTrailData->SpawnedTessellationPoints && CurrentParticle && CurrentTrailData; ++RecalcIdx)
					{
						const float TimeStep = SpawnInvCount * static_cast<float>(RecalcIdx + 1);
						const FVector CurrPosition = CubicInterpVector(
							NextNextSpawnedParticle->Location, NextNextSpawnedTrailData->Tangent,
							NextSpawnedParticle->Location, NewTangent * SpawnDiff,
							TimeStep);
						const FVector CurrTangent = CubicInterpDerivativeVector(
							NextNextSpawnedParticle->Location, NextNextSpawnedTrailData->Tangent,
							NextSpawnedParticle->Location, NewTangent * SpawnDiff,
							TimeStep);

						CurrentParticle->OldLocation = CurrentParticle->Location;
						CurrentParticle->Location = CurrPosition;
						CurrentTrailData->Tangent = CurrTangent * SpawnInvCount;

						if ((RecalcIdx + 1) < NextSpawnedTrailData->SpawnedTessellationPoints)
						{
							const int32 PrevInTrail = TRAIL_EMITTER_GET_PREV(CurrentTrailData->Flags);
							CurrentParticle = (PrevInTrail != TRAIL_EMITTER_NULL_PREV) ? GetParticleDirect(PrevInTrail) : nullptr;
							CurrentTrailData = CurrentParticle ? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(CurrentParticle) + TypeDataOffset) : nullptr;
						}
					}
				}

				LastSourceTangent[TrailIdx] = NewTangent;
			}
		}

		float CurrTimeStep = InvCount;
		for (int32 SpawnIdx = 0; SpawnIdx < MovementSpawnCount; ++SpawnIdx, CurrTimeStep += InvCount)
		{
			const float TimeStep = FMath::Clamp(CurrTimeStep, 0.0f, 1.0f);
			const FVector SpawnPosition = CubicInterpVector(
				LastSourcePosition[TrailIdx], LastSourceTangent[TrailIdx] * Diff,
				CurrentSourcePosition[TrailIdx], CurrentSourceTangent[TrailIdx] * Diff,
				TimeStep);
			const FQuat SpawnRotation = FQuat::Slerp(LastSourceRotation[TrailIdx], CurrentSourceRotation[TrailIdx], TimeStep);
			(void)SpawnRotation;
			const FVector SpawnTangent = CubicInterpDerivativeVector(
				LastSourcePosition[TrailIdx], LastSourceTangent[TrailIdx] * Diff,
				CurrentSourcePosition[TrailIdx], CurrentSourceTangent[TrailIdx] * Diff,
				TimeStep);
			FVector SpawnUp = FVector::ZAxisVector;
			if (TrailTypeData && TrailTypeData->RenderAxis == Trails_SourceUp)
			{
				SpawnUp = FVector::Lerp(LastSourceUp[TrailIdx], CurrentSourceUp[TrailIdx], TimeStep);
			}

			const int32 ParticleIndex = ParticleIndices[ActiveParticles];
			FBaseParticle* Particle = GetParticleDirect(ParticleIndex);
			PreSpawn(Particle, SpawnPosition, FVector::ZeroVector);
			FRibbonTypeDataPayload* TrailData = reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(Particle) + TypeDataOffset);
			SetDeadIndex(TrailData->TrailIndex, ParticleIndex);

			// UE temporarily sets the component-to-world transform to the source
			// position/rotation before running spawn modules. Krafton modules read
			// FSpawnContext::GetTransform from the component; full temporary component
			// transform swapping needs a scene-component transform adapter.
			for (UParticleModule* SpawnModule : LODLevel->SpawnModules)
			{
				if (SpawnModule && SpawnModule->bEnabled)
				{
					SpawnModule->Spawn({ *this, static_cast<int32>(GetModuleDataOffset(SpawnModule)), DeltaTime - (SpawnIdx * Increment), Particle });
				}
			}
			if (LODLevel->TypeDataModule)
			{
				LODLevel->TypeDataModule->Spawn({ *this, TypeDataOffset, DeltaTime - (SpawnIdx * Increment), Particle });
			}

			const float InterpolationPercentage = 1.0f - static_cast<float>(SpawnIdx + 1) / static_cast<float>(MovementSpawnCount);
			FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, DeltaTime - (SpawnIdx * Increment));

			GetParticleLifetimeAndSize(TrailIdx, Particle, bNoLivingParticles, Particle->OneOverMaxLifetime, Particle->Size.X);
			Particle->RelativeTime = (DeltaTime - (SpawnIdx * Increment)) * Particle->OneOverMaxLifetime;
			Particle->Size.Y = Particle->Size.X;
			Particle->BaseSize = Particle->Size;

			TrailData->Flags = TRAIL_EMITTER_SET_NEXT(TrailData->Flags, TRAIL_EMITTER_NULL_NEXT);
			TrailData->Flags = TRAIL_EMITTER_SET_PREV(TrailData->Flags, TRAIL_EMITTER_NULL_PREV);
			TrailData->TrailIndex = TrailIdx;
			TrailData->Tangent = SpawnTangent * InvCount;
			TrailData->SpawnTime = ElapsedTime - (Diff * (1.0f - TimeStep));
			TrailData->SpawnDelta = Diff * TimeStep;
			TrailData->Up = SpawnUp;
			TrailData->SourceIndex = SourceIndices[TrailIdx];
			TrailData->bMovementSpawned = true;
			TrailData->bInterpolatedSpawn = SpawnIdx != (MovementSpawnCount - 1);
			TrailData->SpawnedTessellationPoints = MovementSpawnCount;

			bool bAddedParticle = false;
			if (bNoLivingParticles)
			{
				TrailData->Flags = TRAIL_EMITTER_SET_ONLY(TrailData->Flags);
				TiledUDistanceTraveled[TrailIdx] = 0.0f;
				TrailData->TiledU = 0.0f;
				bNoLivingParticles = false;
				bAddedParticle = true;
				SetStartIndex(TrailData->TrailIndex, ParticleIndex);
			}
			else if (StartParticle)
			{
				bAddedParticle = AddParticleHelper(TrailIdx, StartIndex, StartTrailData, ParticleIndex, TrailData);
			}

			if (bAddedParticle)
			{
				if (bTilingTrail)
				{
					if (StartParticle == nullptr)
					{
						TrailData->TiledU = 0.0f;
					}
					else
					{
						const FVector PositionDelta = Particle->Location - StartParticle->Location;
						TiledUDistanceTraveled[TrailIdx] += PositionDelta.Length();
						TrailData->TiledU = TiledUDistanceTraveled[TrailIdx] / TrailTypeData->TilingDistance;
					}
				}

				StartParticle = Particle;
				StartIndex = ParticleIndex;
				StartTrailData = TrailData;
				++ActiveParticles;
			}
		}

		LastSourcePosition[TrailIdx] = CurrentSourcePosition[TrailIdx];
		LastSourceRotation[TrailIdx] = CurrentSourceRotation[TrailIdx];
		LastSourceTangent[TrailIdx] = CurrentSourceTangent[TrailIdx];
		LastSourceUp[TrailIdx] = CurrentSourceUp[TrailIdx];
		TrailSpawnTimes[TrailIdx] = ElapsedTime;
		LastSourceTimes[TrailIdx] = SourceTimes[TrailIdx];
		if (SourceModule && SourceModule->SourceMethod == PET2SRCM_Particle && SourceTimes[TrailIdx] > 1.0f)
		{
			if (StartTrailData)
			{
				StartTrailData->Flags = TRAIL_EMITTER_SET_DEADTRAIL(StartTrailData->Flags);
				SourceIndices[TrailIdx] = INDEX_NONE;
				SetDeadIndex(StartTrailData->TrailIndex, StartIndex);
			}
		}
	}

	bFirstUpdate = false;
	return bProcessSpawnRate;
}

float FParticleRibbonEmitterInstance::Spawn_RateAndBurst(float DeltaTime)
{
	(void)DeltaTime;
	return SpawnFraction;
}

bool FParticleRibbonEmitterInstance::GetSpawnPerUnitAmount(float DeltaTime, int32 InTrailIdx, int32& OutCount, float& OutRate)
{
	OutCount = 0;
	OutRate = 0.0f;

	if (!SpawnPerUnitModule || InTrailIdx < 0 || InTrailIdx >= static_cast<int32>(SourceDistanceTraveled.size()))
	{
		return false;
	}

	bool bMoved = false;
	float NewTravelLeftover = 0.0f;
	const float UnitScalar = SpawnPerUnitModule->UnitScalar != 0.0f ? SpawnPerUnitModule->UnitScalar : 1.0f;
	const float ParticlesPerUnit = SpawnPerUnitModule->SpawnPerUnit / UnitScalar;

	if (ParticlesPerUnit >= 0.0f)
	{
		float LeftoverTravel = SourceDistanceTraveled[InTrailIdx];
		FVector TravelDirection = CurrentSourcePosition[InTrailIdx] - LastSourcePosition[InTrailIdx];
		if (SpawnPerUnitModule->bIgnoreMovementAlongX) TravelDirection.X = 0.0f;
		if (SpawnPerUnitModule->bIgnoreMovementAlongY) TravelDirection.Y = 0.0f;
		if (SpawnPerUnitModule->bIgnoreMovementAlongZ) TravelDirection.Z = 0.0f;

		float TravelDistance = TravelDirection.Length();
		constexpr float HalfWorldMax = 524288.0f;
		if (((SpawnPerUnitModule->MaxFrameDistance > 0.0f) && (TravelDistance > SpawnPerUnitModule->MaxFrameDistance)) ||
			(TravelDistance > HalfWorldMax))
		{
			// UE original responsibility: clear the per-module SpawnPerUnit instance payload.
			// Missing Krafton foundation: FParticleSpawnPerUnitInstancePayload storage on modules.
			// System to connect later: module instance data for SpawnPerUnit current distance.
			TravelDistance = 0.0f;
			SourceDistanceTraveled[InTrailIdx] = 0.0f;
			LastSourcePosition[InTrailIdx] = CurrentSourcePosition[InTrailIdx];
			LeftoverTravel = 0.0f;
		}

		float CheckTangent = 0.0f;
		if (TrailTypeData && TrailTypeData->TangentSpawningScalar > 0.0f)
		{
			float ElapsedTime = RunningTime;
			if (ActiveParticles == 0)
			{
				if (ElapsedTime == 0.0f)
				{
					ElapsedTime = 1.0e-4f;
				}
				LastSourceTangent[InTrailIdx] = (CurrentSourcePosition[InTrailIdx] - LastSourcePosition[InTrailIdx]) / ElapsedTime;
			}

			const float CurrTangentDivisor = std::max(1.0e-4f, ElapsedTime - TrailSpawnTimes[InTrailIdx]);
			FVector CurrTangent = TravelDirection / CurrTangentDivisor;
			CurrTangent.Normalize();
			FVector PrevTangent = LastSourceTangent[InTrailIdx];
			PrevTangent.Normalize();
			CheckTangent = (CurrTangent.Dot(PrevTangent) - 1.0f) * -0.5f;
		}

		if (TravelDistance > 0.0f)
		{
			if (TravelDistance > (SpawnPerUnitModule->MovementTolerance * UnitScalar))
			{
				bMoved = true;
			}

			TravelDirection.Normalize();

			float NewLeftover = (TravelDistance + LeftoverTravel) * ParticlesPerUnit;
			if (TrailTypeData)
			{
				NewLeftover += CheckTangent * TrailTypeData->TangentSpawningScalar;
			}

			OutCount = (TrailTypeData && TrailTypeData->bSpawnInitialParticle && !ActiveParticles && NewLeftover < 1.0f)
				? 1
				: static_cast<int32>(std::floor(NewLeftover));
			OutRate = DeltaTime > 0.0f ? static_cast<float>(OutCount) / DeltaTime : 0.0f;
			NewTravelLeftover = (TravelDistance + LeftoverTravel) - (static_cast<float>(OutCount) * UnitScalar);
			SourceDistanceTraveled[InTrailIdx] = std::max(0.0f, NewTravelLeftover);
		}
	}

	if (SpawnPerUnitModule->bIgnoreSpawnRateWhenMoving)
	{
		return !bMoved;
	}
	return SpawnPerUnitModule->bProcessSpawnRate;
}

void FParticleRibbonEmitterInstance::UpdateSourceData(float DeltaTime, bool bFirstTime)
{
	const float ElapsedTime = RunningTime;
	const bool bCanBeValidParticleSource = SourceModule && SourceModule->SourceMethod == PET2SRCM_Particle;
	for (int32 TrailIdx = 0; TrailIdx < MaxTrailCount; ++TrailIdx)
	{
		FVector Position;
		FQuat Rotation;
		FVector Up;
		FVector Tangent;
		float TangentStrength = 0.0f;
		const bool bNewSource = SourceIndices[TrailIdx] == INDEX_NONE;
		if (!ResolveSourcePoint(TrailIdx, Position, Rotation, Up, Tangent, TangentStrength))
		{
			continue;
		}

		if (SourceIndices[TrailIdx] == INDEX_NONE && bCanBeValidParticleSource)
		{
			LastSourcePosition[TrailIdx] = Position;
			LastSourceTangent[TrailIdx] = Tangent;
			LastSourceTangentStrength[TrailIdx] = TangentStrength;
			LastSourceRotation[TrailIdx] = Rotation;
			LastSourceUp[TrailIdx] = Up;
			CurrentSourcePosition[TrailIdx] = Position;
			CurrentSourceTangent[TrailIdx] = Tangent;
			CurrentSourceTangentStrength[TrailIdx] = TangentStrength;
			CurrentSourceRotation[TrailIdx] = Rotation;
			CurrentSourceUp[TrailIdx] = Up;
			TrailSpawnTimes[TrailIdx] = 0.0f;
			continue;
		}

		if (bFirstTime || (bNewSource && bCanBeValidParticleSource))
		{
			LastSourcePosition[TrailIdx] = Position;
			LastSourceTangent[TrailIdx] = FVector::ZeroVector;
			LastSourceTangentStrength[TrailIdx] = TangentStrength;
			LastSourceRotation[TrailIdx] = Rotation;
			LastSourceUp[TrailIdx] = Up;
			TrailSpawnTimes[TrailIdx] = RunningTime;
		}

		CurrentSourcePosition[TrailIdx] = Position;
		CurrentSourceRotation[TrailIdx] = Rotation;
		const float ElapsedTimeSinceSpawned = ElapsedTime - TrailSpawnTimes[TrailIdx];
		if (ElapsedTimeSinceSpawned != 0.0f)
		{
			CurrentSourceTangent[TrailIdx] = (CurrentSourcePosition[TrailIdx] - LastSourcePosition[TrailIdx]) / ElapsedTimeSinceSpawned;
		}
		else
		{
			CurrentSourceTangent[TrailIdx] = FVector::XAxisVector;
		}
		CurrentSourceTangentStrength[TrailIdx] = TangentStrength;
		CurrentSourceUp[TrailIdx] = Up;
		if (bFirstTime)
		{
			LastSourceRotation[TrailIdx] = CurrentSourceRotation[TrailIdx];
		}
	}
}

void FParticleRibbonEmitterInstance::ResolveSource()
{
	// UE original responsibility: resolve Actor/Emitter/Particle/default trail source.
	// Missing Jungle foundation: Actor lookup, Emitter lookup, Particle lookup.
	// System to connect later: component instance parameters and emitter source mapping.
}

bool FParticleRibbonEmitterInstance::ResolveSourcePoint(int32 InTrailIdx, FVector& OutPosition, FQuat& OutRotation, FVector& OutUp, FVector& OutTangent, float& OutTangentStrength)
{
	bool bSourceWasSet = false;

	if (SourceModule)
	{
		switch (SourceModule->SourceMethod)
		{
		case PET2SRCM_Particle:
			// UE original responsibility: pick a source particle from SourceEmitter using
			// random/sequential selection, then read its position, velocity and relative time.
			// Missing Krafton foundation: particle source emitter lookup and async emitter instance access.
			// System to connect later: component emitter-name lookup plus TrailSource selection payload.
			break;
		case PET2SRCM_Actor:
			// UE original responsibility: resolve SourceName through component instance parameters,
			// read Actor transform/velocity and optional per-source offsets.
			// Missing Krafton foundation: Actor instance parameters and Actor velocity bridge.
			// System to connect later: particle component instance-parameter lookup.
			break;
		default:
			break;
		}
	}

	if (!bSourceWasSet)
	{
		if (!Component)
		{
			return false;
		}

		OutPosition = Component->GetWorldLocation();
		if (SourceModule && SourceModule->SourceOffsetCount > 0)
		{
			FVector SourceOffsetValue = FVector::ZeroVector;
			if (SourceModule->ResolveSourceOffset(InTrailIdx, this, SourceOffsetValue))
			{
				UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
				if (LODLevel && LODLevel->RequiredModule && !LODLevel->RequiredModule->bUseLocalSpace)
				{
					SourceOffsetValue = Component->GetWorldMatrix().TransformVector(SourceOffsetValue);
				}
				OutPosition += SourceOffsetValue;
			}
		}

		OutRotation = FQuat::FromRotator(Component->GetWorldRotation());
		OutTangent = Component->GetLinearVelocity();
		OutTangentStrength = OutTangent.Dot(OutTangent);
		OutUp = Component->GetUpVector();
		bSourceWasSet = true;
	}

	return bSourceWasSet;
}

void FParticleRibbonEmitterInstance::GetParticleLifetimeAndSize(int32 InTrailIdx, const FBaseParticle* InParticle, bool bInNoLivingParticles, float& OutOneOverMaxLifetime, float& OutSize)
{
	if (InTrailIdx < 0 || InTrailIdx >= static_cast<int32>(CurrentLifetimes.size()) || InTrailIdx >= static_cast<int32>(CurrentSizes.size()))
	{
		OutOneOverMaxLifetime = InParticle ? InParticle->OneOverMaxLifetime : 1.0f;
		OutSize = InParticle ? InParticle->Size.X : 1.0f;
		return;
	}

	if (bInNoLivingParticles)
	{
		UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
		float CurrLifetime = 0.0f;
		for (UParticleModule* SpawnModule : LODLevel->SpawnModules)
		{
			UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(SpawnModule);
			if (LifetimeModule)
			{
				const float MaxLifetime = LifetimeModule->GetLifetimeValue({ *this }, EmitterTime, Component);
				if (CurrLifetime > 0.0f)
				{
					CurrLifetime = 1.0f / (MaxLifetime + (1.0f / CurrLifetime));
				}
				else
				{
					CurrLifetime = (MaxLifetime > 0.0f) ? (1.0f / MaxLifetime) : 0.0f;
				}
				break;
			}
		}

		if (CurrLifetime == 0.0f)
		{
			CurrLifetime = 1.0f;
		}
		if ((1.0f / CurrLifetime) < 0.001f)
		{
			CurrLifetime = 1.0f / 0.001f;
		}

		CurrentLifetimes[InTrailIdx] = CurrLifetime;
		CurrentSizes[InTrailIdx] = InParticle ? InParticle->Size.X : 1.0f;
	}

	OutOneOverMaxLifetime = CurrentLifetimes[InTrailIdx] > 0.0f ? CurrentLifetimes[InTrailIdx] : (InParticle ? InParticle->OneOverMaxLifetime : 1.0f);
	OutSize = CurrentSizes[InTrailIdx] > 0.0f ? CurrentSizes[InTrailIdx] : (InParticle ? InParticle->Size.X : 1.0f);
}

void FParticleRibbonEmitterInstance::Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel)
{
	if (!TrailTypeData || !TrailTypeData->bTangentRecalculationEveryFrame)
	{
		return;
	}

	for (int32 TrailIdx = 0; TrailIdx < MaxTrailCount; ++TrailIdx)
	{
		int32 StartIndex = INDEX_NONE;
		FRibbonTypeDataPayload* StartTrailData = nullptr;
		FBaseParticle* StartParticle = nullptr;
		GetTrailStart<FRibbonTypeDataPayload>(TrailIdx, StartIndex, StartTrailData, StartParticle);

		if (!StartParticle || !StartTrailData || TRAIL_EMITTER_IS_ONLY(StartTrailData->Flags))
		{
			continue;
		}

		FBaseParticle* PrevParticle = StartParticle;
		FRibbonTypeDataPayload* PrevTrailData = StartTrailData;
		FBaseParticle* CurrParticle = nullptr;
		FTrailsBaseTypeDataPayload* TempPayload = nullptr;
		GetParticleInTrail(true, PrevParticle, PrevTrailData, GET_Next, GET_Any, CurrParticle, TempPayload);
		FRibbonTypeDataPayload* CurrTrailData = static_cast<FRibbonTypeDataPayload*>(TempPayload);

		if (CurrParticle)
		{
			TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData,
				PrevTrailData->SpawnTime - CurrTrailData->SpawnTime,
				PrevTrailData);
		}

		while (CurrParticle && CurrTrailData)
		{
			FBaseParticle* NextParticle = nullptr;
			TempPayload = nullptr;
			GetParticleInTrail(true, CurrParticle, CurrTrailData, GET_Next, GET_Any, NextParticle, TempPayload);
			FRibbonTypeDataPayload* NextTrailData = static_cast<FRibbonTypeDataPayload*>(TempPayload);

			if (NextParticle && NextTrailData)
			{
				TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, NextParticle, NextTrailData,
					CurrTrailData->SpawnTime - NextTrailData->SpawnTime,
					CurrTrailData);
			}
			else
			{
				TrailsBase_CalculateTangent(PrevParticle, PrevTrailData, CurrParticle, CurrTrailData,
					PrevTrailData->SpawnTime - CurrTrailData->SpawnTime,
					CurrTrailData);
			}

			PrevParticle = CurrParticle;
			PrevTrailData = CurrTrailData;
			CurrParticle = NextParticle;
			CurrTrailData = NextTrailData;
		}
	}
}

void FParticleRibbonEmitterInstance::DetermineVertexAndTriangleCount()
{
	VertexCount = 0;
	TriangleCount = 0;
	HeadOnlyParticles = 0;
	const int32 Sheets = 1;
	const int32 MaxTessellation = TrailTypeData ? std::max(1, TrailTypeData->MaxTessellationBetweenParticles) : 1;
	const float DistanceStep = TrailTypeData ? TrailTypeData->DistanceTessellationStepSize : 0.0f;
	const float TangentScalar = TrailTypeData ? TrailTypeData->TangentTessellationScalar : 0.0f;
	const bool bScaleTessellation = TrailTypeData && TrailTypeData->bEnableTangentDiffInterpScale;
	const bool bCheckTangentValue = (std::fabs(TangentScalar) > 1.0e-6f) || bScaleTessellation;
	constexpr float ScaleStepFactor = 0.5f;
	int32 TheTrailCount = 0;
	int32 IndexCount = 0;

	for (int32 ii = 0; ii < ActiveParticles; ++ii)
	{
		const int32 ParticleIndex = ParticleIndices[ii];
		FBaseParticle* CurrParticle = GetParticleDirect(ParticleIndex);
		FRibbonTypeDataPayload* CurrTrailData = CurrParticle
			? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(CurrParticle) + TypeDataOffset)
			: nullptr;

		if (!CurrParticle || !CurrTrailData)
		{
			continue;
		}
		else if (TRAIL_EMITTER_IS_HEADONLY(CurrTrailData->Flags))
		{
			CurrTrailData->RenderingInterpCount = 0;
			CurrTrailData->TriangleCount = 0;
			++HeadOnlyParticles;
			continue;
		}

		int32 LocalIndexCount = 0;
		int32 ParticleCount = 0;
		int32 LocalVertexCount = 0;
		bool bProcessParticle = false;

		if (TRAIL_EMITTER_IS_END(CurrTrailData->Flags))
		{
			int32 Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
			if (Prev != TRAIL_EMITTER_NULL_PREV)
			{
				FBaseParticle* PrevParticle = GetParticleDirect(Prev);
				FRibbonTypeDataPayload* PrevTrailData = PrevParticle ? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(PrevParticle) + TypeDataOffset) : nullptr;

				bool bDone = false;
				while (!bDone && PrevParticle && PrevTrailData)
				{
					++ParticleCount;
					float CheckTangent = 0.0f;
					if (bCheckTangentValue)
					{
						const FVector SrcTangent = CurrTrailData->Tangent.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
						const FVector PrevTangent = PrevTrailData->Tangent.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
						CheckTangent = (SrcTangent.Dot(PrevTangent) - 1.0f) * -0.5f;
					}

					float DistDiff = 0.0f;
					if (DistanceStep > 0.0f)
					{
						const float SegmentDistance = FVector::Distance(CurrParticle->Location, PrevParticle->Location);
						DistDiff = SegmentDistance / DistanceStep;
						if (bScaleTessellation && CheckTangent < ScaleStepFactor)
						{
							DistDiff *= 2.0f * FMath::Clamp(CheckTangent, 0.0f, ScaleStepFactor);
						}
					}

					const float TangDiff = CheckTangent * TangentScalar;
					int32 RenderingInterpCount =
						std::min(static_cast<int32>(DistDiff), MaxTessellation) +
						std::min(static_cast<int32>(TangDiff), MaxTessellation);
					RenderingInterpCount = RenderingInterpCount > 0 ? RenderingInterpCount : 1;
					CurrTrailData->RenderingInterpCount = RenderingInterpCount;
					CurrTrailData->PinchScaleFactor = CheckTangent <= 0.5f ? 1.0f : (1.0f - (CheckTangent * 0.5f));

					const int32 TempVertexCount = 2 * CurrTrailData->RenderingInterpCount * Sheets;
					VertexCount += TempVertexCount;
					LocalVertexCount += TempVertexCount;
					LocalIndexCount += TempVertexCount;

					CurrParticle = PrevParticle;
					CurrTrailData = PrevTrailData;
					Prev = TRAIL_EMITTER_GET_PREV(CurrTrailData->Flags);
					if (Prev != TRAIL_EMITTER_NULL_PREV)
					{
						PrevParticle = GetParticleDirect(Prev);
						PrevTrailData = PrevParticle ? reinterpret_cast<FRibbonTypeDataPayload*>(reinterpret_cast<uint8*>(PrevParticle) + TypeDataOffset) : nullptr;
					}
					else
					{
						bDone = true;
					}
				}

				bProcessParticle = true;
			}
		}

		if (bProcessParticle)
		{
			++ParticleCount;
			const int32 TempVertexCount = 2 * Sheets;
			VertexCount += TempVertexCount;
			LocalVertexCount += TempVertexCount;
			LocalIndexCount += TempVertexCount;
			LocalIndexCount += ((Sheets - 1) * 4);

			CurrTrailData->TriangleCount = LocalIndexCount - 2;
			CurrTrailData->RenderingInterpCount = 1;
			IndexCount += LocalIndexCount;
			++TheTrailCount;
		}
	}
	TrailCount = TheTrailCount;
	if (TheTrailCount > 0)
	{
		IndexCount += 4 * (TheTrailCount - 1);
		TriangleCount = IndexCount - (2 * TheTrailCount);
	}
	else
	{
		TriangleCount = 0;
	}
}

bool FParticleRibbonEmitterInstance::IsDynamicDataRequired() const
{
	return FParticleEmitterInstance::IsDynamicDataRequired();
}

FDynamicEmitterDataBase* FParticleRibbonEmitterInstance::GetDynamicData(bool bSelected)
{
	if (TrailTypeData && !TrailTypeData->bRenderGeometry)
	{
		return nullptr;
	}

	FDynamicRibbonEmitterData* Data = new FDynamicRibbonEmitterData();
	if (TrailTypeData)
	{
		Data->bClipSourceSegement = TrailTypeData->bClipSourceSegement;
		Data->bRenderGeometry = TrailTypeData->bRenderGeometry;
		Data->bRenderParticles = TrailTypeData->bRenderSpawnPoints;
		Data->bRenderTangents = TrailTypeData->bRenderTangents;
		Data->bRenderTessellation = TrailTypeData->bRenderTessellation;
		Data->DistanceTessellationStepSize = TrailTypeData->DistanceTessellationStepSize;
		Data->TangentTessellationScalar = TrailTypeData->TangentTessellationScalar;
		Data->RenderAxisOption = TrailTypeData->RenderAxis;
		Data->TextureTileDistance = TrailTypeData->TilingDistance;
		Data->bTextureTileDistance = std::fabs(Data->TextureTileDistance) > 1.0e-6f;
	}
	if (!FillReplayData(Data->Source))
	{
		delete Data;
		return nullptr;
	}
	return Data;
}

void FParticleRibbonEmitterInstance::ApplyWorldOffset(FVector InOffset, bool bWorldShift)
{
	FParticleTrailsEmitterInstance_Base::ApplyWorldOffset(InOffset, bWorldShift);
	for (FVector& Position : CurrentSourcePosition) Position += InOffset;
	for (FVector& Position : LastSourcePosition) Position += InOffset;
}

bool FParticleRibbonEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (!IsReplayType(OutData, EDynamicEmitterType::Ribbon)) return false;
	if (TrailTypeData && !TrailTypeData->bRenderGeometry)
	{
		return false;
	}
	if (ActiveParticles <= 0 || !bEnabled)
	{
		return false;
	}

	// UE original responsibility: DetermineVertexAndTriangleCount writes
	// per-particle trail payload fields such as RenderingInterpCount and
	// TriangleCount, so it must happen before the base replay copy.
	DetermineVertexAndTriangleCount();
	const int32 RibbonIndexCount = TriangleCount + 2;
	if (!FParticleEmitterInstance::FillReplayData(OutData)) return false;
	if (TriangleCount <= 0)
	{
		return false;
	}

	FDynamicRibbonEmitterReplayData& RibbonData = static_cast<FDynamicRibbonEmitterReplayData&>(OutData);
	RibbonData.Material = GetCurrentMaterial();
	RibbonData.bUseLocalSpace = false;
	RibbonData.bLockAxis = false;
	RibbonData.VertexCount = VertexCount;
	RibbonData.PrimitiveCount = TriangleCount;
	RibbonData.IndexCount = RibbonIndexCount;
	RibbonData.TrailDataOffset = TypeDataOffset;
	RibbonData.MaxActiveParticleCount = MaxActiveParticles;
	RibbonData.TrailCount = TrailCount;
	RibbonData.Sheets = TrailTypeData ? std::max(1, TrailTypeData->SheetsPerTrail) : 1;
	RibbonData.MaxTessellationBetweenParticles = TrailTypeData ? TrailTypeData->MaxTessellationBetweenParticles : 0;
	return true;
}
