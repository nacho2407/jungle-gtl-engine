#include "Particle/ParticleEmitterInstance.h"

#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleModules.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
	constexpr int32 MaxParticleIndexValue = static_cast<int32>(std::numeric_limits<uint16>::max());

	int32 AlignParticleBytes(int32 Value)
	{
		return ParticleHelper::AlignParticleSize(Value);
	}

	uint32 FoldToNonZeroSeed(uint64 Value)
	{
		Value ^= Value >> 33;
		Value *= 0xff51afd7ed558ccdull;
		Value ^= Value >> 33;
		Value *= 0xc4ceb9fe1a85ec53ull;
		Value ^= Value >> 33;

		const uint32 Seed = static_cast<uint32>(Value ^ (Value >> 32));
		return Seed != 0u ? Seed : 1u;
	}

	uint32 MakeRuntimeRandomSeed(const void* EmitterTemplate, const void* Instance)
	{
		static uint32 RuntimeSeedCounter = 0u;

		const uint64 TimeSeed = static_cast<uint64>(
			std::chrono::high_resolution_clock::now().time_since_epoch().count());
		const uint64 CounterSeed = static_cast<uint64>(++RuntimeSeedCounter);
		const uint64 EmitterSeed = static_cast<uint64>(reinterpret_cast<std::uintptr_t>(EmitterTemplate));
		const uint64 InstanceSeed = static_cast<uint64>(reinterpret_cast<std::uintptr_t>(Instance));

		// 같은 프레임에 여러 emitter instance가 생성되어도 seed가 겹치지 않도록 시간, counter, pointer 값을 섞습니다.
		return FoldToNonZeroSeed(
			TimeSeed ^
			(CounterSeed * 0x9e3779b97f4a7c15ull) ^
			(EmitterSeed + 0xbf58476d1ce4e5b9ull) ^
			(InstanceSeed + 0x94d049bb133111ebull));
	}

	uint32 GetInitialRandomSeed(
		const UParticleModuleRequired* RequiredModule,
		const void* EmitterTemplate,
		const void* Instance)
	{
		if (RequiredModule != nullptr && RequiredModule->bUseSeededRandom)
		{
			return static_cast<uint32>(std::max(RequiredModule->RandomSeed, 1));
		}

		return MakeRuntimeRandomSeed(EmitterTemplate, Instance);
	}

	FVector AbsVector(const FVector& Value)
	{
		return FVector(std::fabs(Value.X), std::fabs(Value.Y), std::fabs(Value.Z));
	}

	void SetZeroBounds(FVector& OutMin, FVector& OutMax)
	{
		OutMin = FVector::ZeroVector;
		OutMax = FVector::ZeroVector;
	}

	void CalculateScaledFixedBounds(
		const UParticleModuleRequired& RequiredModule,
		FVector& OutMin,
		FVector& OutMax)
	{
		const FVector RawMin = FVector::Min(RequiredModule.FixedBoundsMin, RequiredModule.FixedBoundsMax);
		const FVector RawMax = FVector::Max(RequiredModule.FixedBoundsMin, RequiredModule.FixedBoundsMax);
		const FVector Center = (RawMin + RawMax) * 0.5f;
		const FVector Extent = (RawMax - RawMin) * (0.5f * std::max(RequiredModule.BoundsScale, 0.0f));

		OutMin = Center - Extent;
		OutMax = Center + Extent;
	}

	struct FFloatRange
	{
		float Min = 0.0f;
		float Max = 0.0f;
	};

	struct FVectorRange
	{
		FVector Min = FVector::ZeroVector;
		FVector Max = FVector::ZeroVector;
	};

	FFloatRange GetFloatDistributionRange(const FParticleFloatDistribution& Distribution)
	{
		switch (Distribution.Mode)
		{
		case EParticleDistributionMode::RandomRange:
		case EParticleDistributionMode::RandomRangeCurve:
			return { std::min(Distribution.Min, Distribution.Max), std::max(Distribution.Min, Distribution.Max) };
		case EParticleDistributionMode::Curve:
		case EParticleDistributionMode::Constant:
		default:
			return { Distribution.Constant, Distribution.Constant };
		}
	}

	FVectorRange GetVectorDistributionRange(const FParticleVectorDistribution& Distribution)
	{
		switch (Distribution.Mode)
		{
		case EParticleDistributionMode::RandomRange:
		case EParticleDistributionMode::RandomRangeCurve:
			return { FVector::Min(Distribution.Min, Distribution.Max), FVector::Max(Distribution.Min, Distribution.Max) };
		case EParticleDistributionMode::Curve:
		case EParticleDistributionMode::Constant:
		default:
			return { Distribution.Constant, Distribution.Constant };
		}
	}

	void ExpandDeterministicParticleBounds(FAABB& Bounds, const FParticleLODLevelRuntimeCache& Cache)
	{
		FFloatRange LifetimeRange{ 1.0f, 1.0f };
		FVectorRange LocationRange{ FVector::ZeroVector, FVector::ZeroVector };
		FVectorRange VelocityRange{ FVector::ZeroVector, FVector::ZeroVector };
		FVectorRange SizeRange{ FVector::OneVector, FVector::OneVector };

		for (UParticleModule* Module : Cache.SpawnModules)
		{
			if (Module == nullptr || !Module->bEnabled)
			{
				continue;
			}

			if (const UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
			{
				LifetimeRange = GetFloatDistributionRange(LifetimeModule->Lifetime);
			}
			else if (const UParticleModuleLocation* LocationModule = Cast<UParticleModuleLocation>(Module))
			{
				LocationRange = GetVectorDistributionRange(LocationModule->StartLocation);
			}
			else if (const UParticleModuleVelocity* VelocityModule = Cast<UParticleModuleVelocity>(Module))
			{
				VelocityRange = GetVectorDistributionRange(VelocityModule->StartVelocity);
			}
			else if (const UParticleModuleSize* SizeModule = Cast<UParticleModuleSize>(Module))
			{
				SizeRange = GetVectorDistributionRange(SizeModule->StartSize);
			}
		}

		const float MaxLifetime = std::max(LifetimeRange.Max, 0.0f);
		const FVector TravelMin = VelocityRange.Min * MaxLifetime;
		const FVector TravelMax = VelocityRange.Max * MaxLifetime;
		const FVector HalfSize = FVector::Max(AbsVector(SizeRange.Min), AbsVector(SizeRange.Max)) * 0.5f;

		Bounds.Expand(LocationRange.Min - HalfSize);
		Bounds.Expand(LocationRange.Max + HalfSize);
		Bounds.Expand(LocationRange.Min + FVector::Min(TravelMin, TravelMax) - HalfSize);
		Bounds.Expand(LocationRange.Max + FVector::Max(TravelMin, TravelMax) + HalfSize);
	}
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	Release();
}

bool FParticleEmitterInstance::Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex)
{
	Release();

	if (InTemplate == nullptr || InLODLevelIndex < 0 || InLODLevelIndex >= static_cast<int32>(InTemplate->LODLevels.size()))
	{
		return false;
	}

	SpriteTemplate = InTemplate;
	CurrentLODLevelIndex = InLODLevelIndex;
	CurrentLODLevel = SpriteTemplate->LODLevels[CurrentLODLevelIndex];
	if (CurrentLODLevel == nullptr)
	{
		return false;
	}

	SpriteTemplate->CacheEmitterModuleInfo();
	CurrentRuntimeCache = SpriteTemplate->GetLODLevelRuntimeCache(CurrentLODLevelIndex);
	if (CurrentRuntimeCache == nullptr)
	{
		return false;
	}

	ParticleStride = CurrentRuntimeCache->ParticleStride > 0
		? CurrentRuntimeCache->ParticleStride
		: AlignParticleBytes(static_cast<int32>(sizeof(FBaseParticle)));
	PayloadOffset = CurrentRuntimeCache->PayloadOffset;
	InstancePayloadSize = CurrentRuntimeCache->InstancePayloadSize;

	int32 RequestedMaxParticles = CurrentRuntimeCache->RequiredModule != nullptr
		? CurrentRuntimeCache->RequiredModule->MaxParticles
		: 1; // clamp를 위해 최소값 1로 설정
	if (RequestedMaxParticles > MaxParticleIndexValue)
	{
		UE_LOG_WARNING(
			"[Particle] MaxParticles %d exceeds uint16 ParticleIndices range. Clamped to %d.",
			RequestedMaxParticles,
			MaxParticleIndexValue);
	}
	MaxActiveParticles = std::clamp(RequestedMaxParticles, 1, MaxParticleIndexValue);

	if (!AllocateParticleData(MaxActiveParticles, ParticleStride, InstancePayloadSize))
	{
		Release();
		return false;
	}

	RandomStream.Initialize(GetInitialRandomSeed(CurrentRuntimeCache->RequiredModule, SpriteTemplate, this));
	Reset();
	return true;
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	EmitterTime = 0.0f;
	SecondsSinceCreation = 0.0f;
	bBurstFired = false;

	for (int32 Index = 0; ParticleIndices != nullptr && Index < MaxActiveParticles; ++Index)
	{
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}

	// raw byte block 위에 FBaseParticle 기본값을 다시 써서 이전 시뮬레이션 흔적을 지움
	for (int32 Index = 0; ParticleData != nullptr && Index < MaxActiveParticles; ++Index)
	{
		new (ParticleData + static_cast<size_t>(Index) * ParticleStride) FBaseParticle();
	}

	if (InstanceData != nullptr && InstancePayloadSize > 0)
	{
		std::memset(InstanceData, 0, static_cast<size_t>(InstancePayloadSize));
	}

	RandomStream.Reset();
}

void FParticleEmitterInstance::Release()
{
	ParticleMemoryBlock.clear();
	ParticleMemoryBlock.shrink_to_fit();
	InstanceMemoryBlock.clear();
	InstanceMemoryBlock.shrink_to_fit();
	DataContainer = FParticleDataContainer();

	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceData = nullptr;

	CurrentRuntimeCache = nullptr;
	CurrentLODLevel = nullptr;
	CurrentLODLevelIndex = 0;
	SpriteTemplate = nullptr;

	ParticleStride = 0;
	PayloadOffset = 0;
	InstancePayloadSize = 0;
	ActiveParticles = 0;
	MaxActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	EmitterTime = 0.0f;
	SecondsSinceCreation = 0.0f;
	bBurstFired = false;
}

bool FParticleEmitterInstance::AllocateParticleData(int32 InMaxActiveParticles, int32 InParticleStride, int32 InInstancePayloadSize)
{
	if (InParticleStride <= 0)
	{
		return false;
	}

	const int32 ParticleDataBytes = AlignParticleBytes(InMaxActiveParticles * InParticleStride);
	const int32 ParticleIndexBytes = AlignParticleBytes(InMaxActiveParticles * static_cast<int32>(sizeof(uint16)));
	const int32 TotalParticleMemoryBytes = ParticleDataBytes + ParticleIndexBytes;

	if (TotalParticleMemoryBytes > 0)
	{
		ParticleMemoryBlock.resize(static_cast<size_t>(TotalParticleMemoryBytes + ParticleHelper::ParticleAlignment));
		uint8* AlignedBlockStart = ParticleHelper::AlignParticlePointer(ParticleMemoryBlock.data());
		ParticleData = AlignedBlockStart;
		ParticleIndices = reinterpret_cast<uint16*>(AlignedBlockStart + ParticleDataBytes);
	}

	const int32 AlignedInstancePayloadSize = AlignParticleBytes(InInstancePayloadSize);
	if (AlignedInstancePayloadSize > 0)
	{
		InstanceMemoryBlock.resize(static_cast<size_t>(AlignedInstancePayloadSize + ParticleHelper::ParticleAlignment));
		InstanceData = ParticleHelper::AlignParticlePointer(InstanceMemoryBlock.data());
	}

	DataContainer.MemBlockSize = TotalParticleMemoryBytes;
	DataContainer.ParticleDataNumBytes = ParticleDataBytes;
	DataContainer.ParticleIndicesNumShorts = InMaxActiveParticles;
	DataContainer.ParticleData = ParticleData;
	DataContainer.ParticleIndices = ParticleIndices;
	return true;
}

int32 FParticleEmitterInstance::CalculateSpawnRateCount(float DeltaTime)
{
	if (DeltaTime <= 0.0f || CurrentRuntimeCache == nullptr || CurrentRuntimeCache->SpawnModule == nullptr)
	{
		return 0;
	}

	const UParticleModuleSpawn* SpawnModule = CurrentRuntimeCache->SpawnModule;
	if (!SpawnModule->bProcessSpawnRate)
	{
		SpawnFraction = 0.0f;
		return 0;
	}

	const float SpawnRate = std::max(0.0f, SpawnModule->SpawnRate * SpawnModule->RateScale);
	const float SpawnAmount = SpawnRate * DeltaTime + SpawnFraction;
	const int32 SpawnCount = static_cast<int32>(std::floor(SpawnAmount));
	SpawnFraction = SpawnAmount - static_cast<float>(SpawnCount);
	return std::max(0, SpawnCount);
}

int32 FParticleEmitterInstance::CalculateBurstSpawnCount(float PreviousEmitterTime, float CurrentEmitterTime)
{
	if (CurrentRuntimeCache == nullptr || CurrentRuntimeCache->SpawnModule == nullptr || bBurstFired)
	{
		return 0;
	}

	const UParticleModuleSpawn* SpawnModule = CurrentRuntimeCache->SpawnModule;
	if (!SpawnModule->bProcessBurst || SpawnModule->BurstCount <= 0)
	{
		return 0;
	}

	const float BurstTime = std::max(0.0f, SpawnModule->BurstTime);
	if (PreviousEmitterTime <= BurstTime && CurrentEmitterTime >= BurstTime)
	{
		bBurstFired = true;
		return SpawnModule->BurstCount;
	}

	return 0;
}

int32 FParticleEmitterInstance::SpawnParticles(int32 Count, float DeltaTime)
{
	if (Count <= 0 || ParticleData == nullptr || ParticleIndices == nullptr)
	{
		return 0;
	}

	const int32 AvailableSlots = MaxActiveParticles - ActiveParticles;
	const int32 SpawnCount = std::clamp(Count, 0, AvailableSlots);
	if (SpawnCount <= 0)
	{
		return 0;
	}

	const float SpawnInterval = SpawnCount > 0 ? DeltaTime / static_cast<float>(SpawnCount) : 0.0f;

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		const int32 ActiveIndex = ActiveParticles;
		const int32 PhysicalIndex = ParticleIndices[ActiveIndex];
		FBaseParticle& Particle = GetParticleByPhysicalIndex(PhysicalIndex);
		new (&Particle) FBaseParticle();

		Particle.Seed = RandomStream.GetUnsignedInt();
		Particle.OldLocation = Particle.Location;
		Particle.RelativeTime = 0.0f;
		Particle.Lifetime = std::max(Particle.Lifetime, 0.0001f);
		Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;

		const float SpawnTime = SpawnInterval * static_cast<float>(SpawnIndex);
		for (UParticleModule* Module : CurrentRuntimeCache->SpawnModules)
		{
			if (Module == nullptr || !Module->bEnabled)
			{
				continue;
			}

			const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(Module);
			Module->Spawn(this, Offset, SpawnTime, Particle);
		}

		Particle.Lifetime = std::max(Particle.Lifetime, 0.0001f);
		Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
		Particle.OldLocation = Particle.Location;

		FParticleEventSpawnData Event;
		Event.ParticleIndex = PhysicalIndex;
		Event.Location = GetParticleLocationForRender(Particle);
		Owner.AddSpawnEvent(Event);

		++ActiveParticles;
		++ParticleCounter;
	}

	return SpawnCount;
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	int32 ActiveIndex = 0;
	while (ActiveIndex < ActiveParticles)
	{
		FBaseParticle& Particle = GetParticleByActiveIndex(ActiveIndex);
		Particle.OldLocation = Particle.Location;
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Rotation += Particle.RotationRate * DeltaTime;
		Particle.RelativeTime += DeltaTime * Particle.OneOverMaxLifetime;

		if (Particle.RelativeTime >= 1.0f)
		{
			KillParticle(ActiveIndex);
			continue;
		}

		++ActiveIndex;
	}
}

void FParticleEmitterInstance::KillParticle(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || ParticleIndices == nullptr)
	{
		return;
	}

	const int32 LastActiveIndex = ActiveParticles - 1;
	const uint16 KilledPhysicalIndex = ParticleIndices[ActiveIndex];
	FBaseParticle& Particle = GetParticleByPhysicalIndex(KilledPhysicalIndex);

	FParticleEventDeathData Event;
	Event.ParticleIndex = KilledPhysicalIndex;
	Event.Location = GetParticleLocationForRender(Particle);
	Owner.AddDeathEvent(Event);

	// particle memory는 옮기지 않고 active index 목록만 swap-remove해서 제거(속도 최적화)
	ParticleIndices[ActiveIndex] = ParticleIndices[LastActiveIndex];
	ParticleIndices[LastActiveIndex] = KilledPhysicalIndex;
	--ActiveParticles;
}

FBaseParticle& FParticleEmitterInstance::GetParticleByActiveIndex(int32 ActiveIndex)
{
	assert(ActiveIndex >= 0 && ActiveIndex < ActiveParticles);
	const int32 PhysicalIndex = ParticleIndices[ActiveIndex];
	return GetParticleByPhysicalIndex(PhysicalIndex);
}

const FBaseParticle& FParticleEmitterInstance::GetParticleByActiveIndex(int32 ActiveIndex) const
{
	assert(ActiveIndex >= 0 && ActiveIndex < ActiveParticles);
	const int32 PhysicalIndex = ParticleIndices[ActiveIndex];
	return GetParticleByPhysicalIndex(PhysicalIndex);
}

FBaseParticle& FParticleEmitterInstance::GetParticleByPhysicalIndex(int32 PhysicalIndex)
{
	assert(ParticleData != nullptr);
	assert(PhysicalIndex >= 0 && PhysicalIndex < MaxActiveParticles);
	return *reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(PhysicalIndex) * ParticleStride);
}

const FBaseParticle& FParticleEmitterInstance::GetParticleByPhysicalIndex(int32 PhysicalIndex) const
{
	assert(ParticleData != nullptr);
	assert(PhysicalIndex >= 0 && PhysicalIndex < MaxActiveParticles);
	return *reinterpret_cast<const FBaseParticle*>(ParticleData + static_cast<size_t>(PhysicalIndex) * ParticleStride);
}

uint8* FParticleEmitterInstance::GetParticlePayloadByOffset(FBaseParticle& Particle, int32 Offset)
{
	if (Offset < 0 || Offset >= ParticleStride)
	{
		return nullptr;
	}

	return reinterpret_cast<uint8*>(&Particle) + Offset;
}

const uint8* FParticleEmitterInstance::GetParticlePayloadByOffset(const FBaseParticle& Particle, int32 Offset) const
{
	if (Offset < 0 || Offset >= ParticleStride)
	{
		return nullptr;
	}

	return reinterpret_cast<const uint8*>(&Particle) + Offset;
}

uint8* FParticleEmitterInstance::GetParticlePayload(FBaseParticle& Particle, UParticleModule* Module)
{
	if (CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	return GetParticlePayloadByOffset(Particle, CurrentRuntimeCache->GetParticlePayloadOffset(Module));
}

const uint8* FParticleEmitterInstance::GetParticlePayload(const FBaseParticle& Particle, UParticleModule* Module) const
{
	if (CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	return GetParticlePayloadByOffset(Particle, CurrentRuntimeCache->GetParticlePayloadOffset(Module));
}

uint8* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* Module)
{
	if (InstanceData == nullptr || CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	const int32 Offset = CurrentRuntimeCache->GetInstancePayloadOffset(Module);
	if (Offset < 0 || Offset >= InstancePayloadSize)
	{
		return nullptr;
	}

	return InstanceData + Offset;
}

const uint8* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* Module) const
{
	if (InstanceData == nullptr || CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	const int32 Offset = CurrentRuntimeCache->GetInstancePayloadOffset(Module);
	if (Offset < 0 || Offset >= InstancePayloadSize)
	{
		return nullptr;
	}

	return InstanceData + Offset;
}

bool FParticleEmitterInstance::UsesLocalSpace() const
{
	return CurrentRuntimeCache == nullptr ||
		CurrentRuntimeCache->RequiredModule == nullptr ||
		CurrentRuntimeCache->RequiredModule->CoordinateSpace == EParticleCoordinateSpace::Local;
}

FVector FParticleEmitterInstance::TransformLocationToSimulationSpace(const FVector& WorldLocation) const
{
	if (!UsesLocalSpace())
	{
		return WorldLocation;
	}

	return Owner.GetComponentToWorld().GetInverse().TransformPosition(WorldLocation);
}

FVector FParticleEmitterInstance::TransformVelocityToSimulationSpace(const FVector& WorldVelocity) const
{
	if (!UsesLocalSpace())
	{
		return WorldVelocity;
	}

	return Owner.GetComponentToWorld().GetInverse().TransformVector(WorldVelocity);
}

FVector FParticleEmitterInstance::GetParticleLocationForRender(const FBaseParticle& Particle) const
{
	// particle의 simulation space와 관계 없이 항상 render-ready world space를 반환
	return UsesLocalSpace()
		? Owner.GetComponentToWorld().TransformPosition(Particle.Location)
		: Particle.Location;
}

void FParticleEmitterInstance::CalculateLocalBounds(FVector& OutMin, FVector& OutMax) const
{
	const UParticleModuleRequired* RequiredModule = CurrentRuntimeCache != nullptr
		? CurrentRuntimeCache->RequiredModule
		: nullptr;
	if (RequiredModule != nullptr && RequiredModule->bUseFixedBounds)
	{
		CalculateScaledFixedBounds(*RequiredModule, OutMin, OutMax);
		return;
	}

	FAABB Bounds;
	ExpandDeterministicParticleBounds(Bounds, *CurrentRuntimeCache);
	if (!Bounds.IsValid())
	{
		SetZeroBounds(OutMin, OutMax);
		return;
	}

	OutMin = Bounds.Min;
	OutMax = Bounds.Max;
}

void FParticleEmitterInstance::CalculateWorldBounds(FVector& OutMin, FVector& OutMax) const
{
	CalculateLocalBounds(OutMin, OutMax);
	if (!UsesLocalSpace())
	{
		return;
	}

	const FAABB LocalBounds(OutMin, OutMax);
	const FAABB WorldBounds = FAABB::TransformAABB(LocalBounds, Owner.GetComponentToWorld());
	OutMin = WorldBounds.Min;
	OutMax = WorldBounds.Max;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (CurrentLODLevel == nullptr || CurrentRuntimeCache == nullptr || !CurrentLODLevel->bEnabled)
	{
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const float PreviousEmitterTime = EmitterTime;
	EmitterTime += DeltaTime;
	SecondsSinceCreation += DeltaTime;

	const int32 BurstSpawnCount = CalculateBurstSpawnCount(PreviousEmitterTime, EmitterTime);
	const int32 ActualBurstSpawnCount = SpawnParticles(BurstSpawnCount, DeltaTime);
	if (ActualBurstSpawnCount > 0)
	{
		FParticleEventBurstData Event;
		Event.SpawnCount = ActualBurstSpawnCount;
		Event.Location = Owner.GetWorldLocation();
		Owner.AddBurstEvent(Event);
	}

	SpawnParticles(CalculateSpawnRateCount(DeltaTime), DeltaTime);
	UpdateParticles(DeltaTime);

	for (UParticleModule* Module : CurrentRuntimeCache->UpdateModules)
	{
		if (Module != nullptr && Module->bEnabled)
		{
			const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(Module);
			Module->Update(this, Offset, DeltaTime);
		}
	}
}
