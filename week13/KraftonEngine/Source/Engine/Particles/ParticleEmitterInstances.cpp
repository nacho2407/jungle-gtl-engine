#include "Particles/ParticleEmitterInstances.h"

#include "Particles/ParticleMemory.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleEmitter.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
	int32 ClampParticleCountToUInt16(int32 Count)
	{
		return std::min<int32>(Count, std::numeric_limits<uint16>::max());
	}

	bool IsReplayType(const FDynamicEmitterReplayDataBase& Data, EDynamicEmitterType ExpectedType)
	{
		return Data.eEmitterType == ExpectedType;
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

	return true;
}

uint32 FParticleMeshEmitterInstance::RequiredBytes()
{
	uint32 Bytes = FParticleEmitterInstance::RequiredBytes();

	if (bMeshRotationActive)
	{
		MeshRotationOffset = PayloadOffset + static_cast<int32>(Bytes);
		Bytes += sizeof(FMeshRotationPayloadData);
	}
	else
	{
		MeshRotationOffset = 0;
	}

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

	if (bMeshRotationActive && bEnabled && MeshRotationOffset > 0)
	{
		for (int32 i = 0; i < ActiveParticles; ++i)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
			FMeshRotationPayloadData* Payload =
				reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(&Particle) + MeshRotationOffset);
			Payload->RotationRate = Payload->RotationRateBase;
			if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
				Payload->Rotation = Payload->InitialOrientation + Payload->CurContinuousRotation;
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
	// Phase1 has no static mesh bounds, so keep the base integrator/bounds path.
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);
}

void FParticleMeshEmitterInstance::PreSpawn(
	FBaseParticle* Particle,
	const FVector& InitialLocation,
	const FVector& InitialVelocity)
{
	FParticleEmitterInstance::PreSpawn(Particle, InitialLocation, InitialVelocity);

	uint8* ParticleBase = reinterpret_cast<uint8*>(Particle);

	if (MeshRotationOffset > 0)
	{
		FMeshRotationPayloadData& RotationPayload =
			*reinterpret_cast<FMeshRotationPayloadData*>(ParticleBase + MeshRotationOffset);
		RotationPayload.InitialOrientation = FVector::ZeroVector;
		RotationPayload.Rotation = FVector::ZeroVector;
		RotationPayload.CurContinuousRotation = FVector::ZeroVector;
		RotationPayload.RotationRateBase = FVector::ZeroVector;
		RotationPayload.RotationRate = FVector::ZeroVector;
	}

	if (MeshMotionBlurOffset > 0)
	{
		FMeshMotionBlurPayloadData& MotionBlurPayload =
			*reinterpret_cast<FMeshMotionBlurPayloadData*>(ParticleBase + MeshMotionBlurOffset);
		MotionBlurPayload.BaseParticlePrevVelocity = InitialVelocity;
		MotionBlurPayload.BaseParticlePrevSize = Particle->Size;
		MotionBlurPayload.BaseParticlePrevRotation = Particle->Rotation;
		MotionBlurPayload.PayloadPrevRotation = FVector::ZeroVector;
	}
}

void FParticleMeshEmitterInstance::PostSpawn(
	FBaseParticle* Particle,
	float InterpolationPercentage,
	float SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, SpawnTime);

	if (MeshMotionBlurOffset > 0)
	{
		uint8* ParticleBase = reinterpret_cast<uint8*>(Particle);
		FMeshMotionBlurPayloadData& MotionBlurPayload =
			*reinterpret_cast<FMeshMotionBlurPayloadData*>(ParticleBase + MeshMotionBlurOffset);
		MotionBlurPayload.BaseParticlePrevVelocity = Particle->Velocity;
		MotionBlurPayload.BaseParticlePrevSize = Particle->Size;
		MotionBlurPayload.BaseParticlePrevRotation = Particle->Rotation;
	}
}

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
	if (!IsDynamicDataRequired())
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
	MeshData.MeshRotationOffset = MeshRotationOffset;
	MeshData.MeshMotionBlurOffset = MeshMotionBlurOffset;
	MeshData.bEnableMotionBlur = bMotionBlurEnabled;
	MeshData.SubUVDataOffset = SubUVDataOffset;
	MeshData.DynamicParameterDataOffset = DynamicParameterDataOffset;
	MeshData.LightDataOffset = LightDataOffset;
	MeshData.OrbitModuleOffset = OrbitModuleOffset;
	MeshData.CameraPayloadOffset = CameraPayloadOffset;
	MeshData.bUseLocalSpace = GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;

	return true;
}
