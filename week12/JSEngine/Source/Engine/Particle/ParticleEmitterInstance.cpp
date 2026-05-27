#include "Geometry/AABB.h"
#include "Particle/ParticleEmitterInstance.h"

#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Core/ResourceManager.h"
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
	constexpr int32 CurveBoundsSampleSteps = 32;

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

	FVector MakeUniformVector(float Value)
	{
		return FVector(Value, Value, Value);
	}

	FVector ApplyVectorDistributionMode(const FParticleVectorDistribution& Distribution, const FVector& Value)
	{
		return Distribution.VectorMode == EParticleVectorDistributionMode::UniformXYZ
			? MakeUniformVector(Value.X)
			: Value;
	}

	void ExpandFloatRange(FFloatRange& Range, float Value)
	{
		Range.Min = std::min(Range.Min, Value);
		Range.Max = std::max(Range.Max, Value);
	}

	void ExpandVectorRange(FVectorRange& Range, const FVector& Value)
	{
		Range.Min = FVector::Min(Range.Min, Value);
		Range.Max = FVector::Max(Range.Max, Value);
	}

	UCurveFloatAsset* ResolveFloatCurve(const TSoftObjectPtr<UCurveFloatAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadFloatCurve(Path);
	}

	UCurveVectorAsset* ResolveVectorCurve(const TSoftObjectPtr<UCurveVectorAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadVectorCurve(Path);
	}

	float GetCurveBoundsSampleTime(float CurveStartTime, float CurveEndTime, int32 SampleIndex)
	{
		const float SafeEndTime = std::max(CurveStartTime, CurveEndTime);
		if (SafeEndTime <= CurveStartTime)
		{
			return CurveStartTime;
		}

		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(CurveBoundsSampleSteps);
		return CurveStartTime + (SafeEndTime - CurveStartTime) * Alpha;
	}

	FFloatRange SampleFloatCurveRange(
		UCurveFloatAsset* Curve,
		float CurveStartTime,
		float CurveEndTime,
		float Fallback)
	{
		if (Curve == nullptr)
		{
			return { Fallback, Fallback };
		}

		const float FirstValue = Curve->Evaluate(CurveStartTime);
		FFloatRange Range{ FirstValue, FirstValue };
		for (int32 SampleIndex = 1; SampleIndex <= CurveBoundsSampleSteps; ++SampleIndex)
		{
			ExpandFloatRange(Range, Curve->Evaluate(GetCurveBoundsSampleTime(CurveStartTime, CurveEndTime, SampleIndex)));
		}
		return Range;
	}

	FFloatRange SampleFloatCurvePairRange(
		UCurveFloatAsset* MinCurve,
		UCurveFloatAsset* MaxCurve,
		float CurveStartTime,
		float CurveEndTime,
		float MinFallback,
		float MaxFallback)
	{
		const float FirstMin = MinCurve != nullptr ? MinCurve->Evaluate(CurveStartTime) : MinFallback;
		const float FirstMax = MaxCurve != nullptr ? MaxCurve->Evaluate(CurveStartTime) : MaxFallback;
		FFloatRange Range{ std::min(FirstMin, FirstMax), std::max(FirstMin, FirstMax) };

		for (int32 SampleIndex = 1; SampleIndex <= CurveBoundsSampleSteps; ++SampleIndex)
		{
			const float Time = GetCurveBoundsSampleTime(CurveStartTime, CurveEndTime, SampleIndex);
			ExpandFloatRange(Range, MinCurve != nullptr ? MinCurve->Evaluate(Time) : MinFallback);
			ExpandFloatRange(Range, MaxCurve != nullptr ? MaxCurve->Evaluate(Time) : MaxFallback);
		}
		return Range;
	}

	FVectorRange SampleVectorCurveRange(
		const FParticleVectorDistribution& Distribution,
		UCurveVectorAsset* Curve,
		float CurveStartTime,
		float CurveEndTime,
		const FVector& Fallback)
	{
		if (Curve == nullptr)
		{
			const FVector Value = ApplyVectorDistributionMode(Distribution, Fallback);
			return { Value, Value };
		}

		const FVector FirstValue = ApplyVectorDistributionMode(Distribution, Curve->Evaluate(CurveStartTime));
		FVectorRange Range{ FirstValue, FirstValue };
		for (int32 SampleIndex = 1; SampleIndex <= CurveBoundsSampleSteps; ++SampleIndex)
		{
			const float Time = GetCurveBoundsSampleTime(CurveStartTime, CurveEndTime, SampleIndex);
			ExpandVectorRange(Range, ApplyVectorDistributionMode(Distribution, Curve->Evaluate(Time)));
		}
		return Range;
	}

	FVectorRange SampleVectorCurvePairRange(
		const FParticleVectorDistribution& Distribution,
		UCurveVectorAsset* MinCurve,
		UCurveVectorAsset* MaxCurve,
		float CurveStartTime,
		float CurveEndTime,
		const FVector& MinFallback,
		const FVector& MaxFallback)
	{
		const FVector FirstMin = ApplyVectorDistributionMode(
			Distribution,
			MinCurve != nullptr ? MinCurve->Evaluate(CurveStartTime) : MinFallback);
		const FVector FirstMax = ApplyVectorDistributionMode(
			Distribution,
			MaxCurve != nullptr ? MaxCurve->Evaluate(CurveStartTime) : MaxFallback);
		FVectorRange Range{ FVector::Min(FirstMin, FirstMax), FVector::Max(FirstMin, FirstMax) };

		for (int32 SampleIndex = 1; SampleIndex <= CurveBoundsSampleSteps; ++SampleIndex)
		{
			const float Time = GetCurveBoundsSampleTime(CurveStartTime, CurveEndTime, SampleIndex);
			ExpandVectorRange(
				Range,
				ApplyVectorDistributionMode(Distribution, MinCurve != nullptr ? MinCurve->Evaluate(Time) : MinFallback));
			ExpandVectorRange(
				Range,
				ApplyVectorDistributionMode(Distribution, MaxCurve != nullptr ? MaxCurve->Evaluate(Time) : MaxFallback));
		}
		return Range;
	}

	FFloatRange GetFloatDistributionRange(
		const FParticleFloatDistribution& Distribution,
		float CurveStartTime,
		float CurveEndTime)
	{
		switch (Distribution.Mode)
		{
		case EParticleDistributionMode::RandomRange:
			return { std::min(Distribution.Min, Distribution.Max), std::max(Distribution.Min, Distribution.Max) };
		case EParticleDistributionMode::Curve:
			return SampleFloatCurveRange(
				ResolveFloatCurve(Distribution.Curve),
				CurveStartTime,
				CurveEndTime,
				Distribution.Constant);
		case EParticleDistributionMode::RandomRangeCurve:
			return SampleFloatCurvePairRange(
				ResolveFloatCurve(Distribution.MinCurve),
				ResolveFloatCurve(Distribution.MaxCurve),
				CurveStartTime,
				CurveEndTime,
				Distribution.Min,
				Distribution.Max);
		case EParticleDistributionMode::Constant:
		default:
			return { Distribution.Constant, Distribution.Constant };
		}
	}

	FVectorRange GetVectorDistributionRange(
		const FParticleVectorDistribution& Distribution,
		float CurveStartTime,
		float CurveEndTime)
	{
		const bool bUniformXYZ = Distribution.VectorMode == EParticleVectorDistributionMode::UniformXYZ;
		switch (Distribution.Mode)
		{
		case EParticleDistributionMode::RandomRange:
			if (bUniformXYZ)
			{
				const float MinValue = std::min(Distribution.Min.X, Distribution.Max.X);
				const float MaxValue = std::max(Distribution.Min.X, Distribution.Max.X);
				return { FVector(MinValue, MinValue, MinValue), FVector(MaxValue, MaxValue, MaxValue) };
			}
			else
			{
				return { FVector::Min(Distribution.Min, Distribution.Max), FVector::Max(Distribution.Min, Distribution.Max) };
			}
		case EParticleDistributionMode::Curve:
			return SampleVectorCurveRange(
				Distribution,
				ResolveVectorCurve(Distribution.Curve),
				CurveStartTime,
				CurveEndTime,
				Distribution.Constant);
		case EParticleDistributionMode::RandomRangeCurve:
			return SampleVectorCurvePairRange(
				Distribution,
				ResolveVectorCurve(Distribution.MinCurve),
				ResolveVectorCurve(Distribution.MaxCurve),
				CurveStartTime,
				CurveEndTime,
				Distribution.Min,
				Distribution.Max);
		case EParticleDistributionMode::Constant:
		default:
			if (bUniformXYZ)
			{
				return {
					FVector(Distribution.Constant.X, Distribution.Constant.X, Distribution.Constant.X),
					FVector(Distribution.Constant.X, Distribution.Constant.X, Distribution.Constant.X)
				};
			}
			else
			{
				return { Distribution.Constant, Distribution.Constant };
			}
		}
	}

	void ExpandDeterministicParticleBounds(FAABB& Bounds, const FParticleLODLevelRuntimeCache& Cache)
	{
		FFloatRange LifetimeRange{ 1.0f, 1.0f };
		FVectorRange LocationRange{ FVector::ZeroVector, FVector::ZeroVector };
		FVectorRange VelocityRange{ FVector::ZeroVector, FVector::ZeroVector };
		FVectorRange SizeRange{ FVector::OneVector, FVector::OneVector };
		const float CurveStartTime = 0.0f;
		const float CurveEndTime = Cache.RequiredModule != nullptr
			? std::max(Cache.RequiredModule->EmitterDuration, 0.0f)
			: CurveStartTime;

		for (UParticleModule* Module : Cache.SpawnModules)
		{
			if (Module == nullptr || !Module->bEnabled)
			{
				continue;
			}

			if (const UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
			{
				// Spawn 모듈의 curve time은 loop-local SpawnTime이므로 emitter duration 전체를 샘플링합니다.
				LifetimeRange = GetFloatDistributionRange(LifetimeModule->Lifetime, CurveStartTime, CurveEndTime);
			}
			else if (const UParticleModuleLocation* LocationModule = Cast<UParticleModuleLocation>(Module))
			{
				LocationRange = GetVectorDistributionRange(LocationModule->StartLocation, CurveStartTime, CurveEndTime);
			}
			else if (const UParticleModuleVelocity* VelocityModule = Cast<UParticleModuleVelocity>(Module))
			{
				VelocityRange = GetVectorDistributionRange(VelocityModule->StartVelocity, CurveStartTime, CurveEndTime);
			}
			else if (const UParticleModuleSize* SizeModule = Cast<UParticleModuleSize>(Module))
			{
				SizeRange = GetVectorDistributionRange(SizeModule->StartSize, CurveStartTime, CurveEndTime);
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

	if (InTemplate == nullptr || InTemplate->LODLevels.empty())
	{
		return false;
	}

	SpriteTemplate = InTemplate;
	SpriteTemplate->CacheEmitterModuleInfo();

	const FParticleLODLevelRuntimeCache* LayoutRuntimeCache = SpriteTemplate->GetLOD0RuntimeCache();
	if (LayoutRuntimeCache == nullptr)
	{
		return false;
	}

	ParticleStride = LayoutRuntimeCache->ParticleStride > 0
		? LayoutRuntimeCache->ParticleStride
		: AlignParticleBytes(static_cast<int32>(sizeof(FBaseParticle)));
	PayloadOffset = LayoutRuntimeCache->PayloadOffset;
	InstancePayloadSize = LayoutRuntimeCache->InstancePayloadSize;

	int32 RequestedMaxParticles = LayoutRuntimeCache->RequiredModule != nullptr
		? LayoutRuntimeCache->RequiredModule->MaxParticles
		: 1; // clamp 최소값
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

	const int32 InitialLODIndex = (InLODLevelIndex >= 0 && InLODLevelIndex < static_cast<int32>(SpriteTemplate->LODLevels.size()))
		? InLODLevelIndex
		: 0;
	if (!SetCurrentLODIndex(InitialLODIndex) && !SetCurrentLODIndex(0))
	{
		Release();
		return false;
	}

	RandomStream.Initialize(GetInitialRandomSeed(GetRequiredModule(), SpriteTemplate, this));
	Reset();
	return true;
}

bool FParticleEmitterInstance::SetCurrentLODIndex(int32 InLODLevelIndex)
{
	if (SpriteTemplate == nullptr ||
		InLODLevelIndex < 0 ||
		InLODLevelIndex >= static_cast<int32>(SpriteTemplate->LODLevels.size()))
	{
		return false;
	}

	// invalid topology의 lower LOD 차단
	if (InLODLevelIndex != 0 && !SpriteTemplate->ValidateLODTopology(false))
	{
		return false;
	}

	UParticleLODLevel* NewLODLevel = SpriteTemplate->LODLevels[static_cast<size_t>(InLODLevelIndex)];
	const FParticleLODLevelRuntimeCache* NewRuntimeCache = SpriteTemplate->GetLODLevelRuntimeCache(InLODLevelIndex);
	if (NewLODLevel == nullptr || NewRuntimeCache == nullptr)
	{
		return false;
	}

	// storage layout 불변성
	if (ParticleStride > 0 && NewRuntimeCache->ParticleStride != ParticleStride)
	{
		UE_LOG_WARNING("[Particle] LOD switch rejected because particle stride differs from the allocated storage.");
		return false;
	}
	if (NewRuntimeCache->InstancePayloadSize != InstancePayloadSize)
	{
		UE_LOG_WARNING("[Particle] LOD switch rejected because instance payload size differs from the allocated storage.");
		return false;
	}

	CurrentLODLevelIndex = InLODLevelIndex;
	CurrentLODLevel = NewLODLevel;
	CurrentRuntimeCache = NewRuntimeCache;

	const size_t NewBurstCount =
		CurrentRuntimeCache->SpawnModule != nullptr
			? CurrentRuntimeCache->SpawnModule->BurstList.size()
			: 0;
	if (BurstFiredThisLoop.size() != NewBurstCount)
	{
		// burst fired state layout
		ResetBurstFiredState();
	}

	return true;
}

bool FParticleEmitterInstance::RefreshCurrentRuntimeCache()
{
	if (SpriteTemplate == nullptr ||
		CurrentLODLevelIndex < 0 ||
		CurrentLODLevelIndex >= static_cast<int32>(SpriteTemplate->LODLevels.size()))
	{
		CurrentLODLevel = nullptr;
		CurrentRuntimeCache = nullptr;
		return false;
	}

	UParticleLODLevel* NewLODLevel = SpriteTemplate->LODLevels[static_cast<size_t>(CurrentLODLevelIndex)];
	const FParticleLODLevelRuntimeCache* NewRuntimeCache = SpriteTemplate->GetLODLevelRuntimeCache(CurrentLODLevelIndex);
	if (NewLODLevel == nullptr || NewRuntimeCache == nullptr)
	{
		CurrentLODLevel = nullptr;
		CurrentRuntimeCache = nullptr;
		return false;
	}

	if (ParticleStride > 0 && NewRuntimeCache->ParticleStride != ParticleStride)
	{
		return false;
	}
	if (NewRuntimeCache->InstancePayloadSize != InstancePayloadSize)
	{
		return false;
	}

	CurrentLODLevel = NewLODLevel;
	CurrentRuntimeCache = NewRuntimeCache;
	return true;
}

void FParticleEmitterInstance::Reset()
{
	ActiveParticles = 0;
	ParticleCounter = 0;
	SecondsSinceCreation = 0.0f;
	ResetLoopRuntimeState();

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
	CompletedLoopCount = 0;
	bEmitterSpawnComplete = false;
	BurstFiredThisLoop.clear();
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

const UParticleModuleRequired* FParticleEmitterInstance::GetRequiredModule() const
{
	return CurrentRuntimeCache != nullptr ? CurrentRuntimeCache->RequiredModule : nullptr;
}

float FParticleEmitterInstance::GetEmitterDuration() const
{
	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	return RequiredModule != nullptr ? RequiredModule->EmitterDuration : 0.0f;
}

int32 FParticleEmitterInstance::GetTotalLoopCount() const
{
	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	if (RequiredModule == nullptr)
	{
		return 0;
	}

	return RequiredModule->bEmitterLoops
		? std::max(RequiredModule->MaxEmitterLoops, 0)
		: 1;
}

bool FParticleEmitterInstance::IsInfiniteLooping() const
{
	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	return RequiredModule != nullptr && RequiredModule->bInfiniteEmitterLoops;
}

bool FParticleEmitterInstance::CanSpawnEmitter() const
{
	const bool bInfiniteLooping = IsInfiniteLooping();
	return !bEmitterSpawnComplete &&
		CurrentRuntimeCache != nullptr &&
		CurrentRuntimeCache->SpawnModule != nullptr &&
		GetEmitterDuration() > 0.0f &&
		(bInfiniteLooping || (GetTotalLoopCount() > 0 && CompletedLoopCount < GetTotalLoopCount()));
}

void FParticleEmitterInstance::ResetLoopRuntimeState()
{
	SpawnFraction = 0.0f;
	EmitterTime = 0.0f;
	CompletedLoopCount = 0;
	bEmitterSpawnComplete = false;
	ResetBurstFiredState();
}

void FParticleEmitterInstance::ResetBurstFiredState()
{
	const UParticleModuleSpawn* SpawnModule = CurrentRuntimeCache != nullptr
		? CurrentRuntimeCache->SpawnModule
		: nullptr;
	const int32 BurstEntryCount = SpawnModule != nullptr
		? static_cast<int32>(SpawnModule->BurstList.size())
		: 0;

	BurstFiredThisLoop.clear();
	BurstFiredThisLoop.resize(static_cast<size_t>(std::max(BurstEntryCount, 0)), 0u);
}

int32 FParticleEmitterInstance::GetCurrentLODMaxParticles() const
{
	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	const int32 CurrentLODMaxParticles = RequiredModule != nullptr
		? RequiredModule->MaxParticles
		: MaxActiveParticles;

	return std::clamp(CurrentLODMaxParticles, 1, std::max(MaxActiveParticles, 1));
}

void FParticleEmitterInstance::CompleteEmitterLoop()
{
	++CompletedLoopCount;
	SpawnFraction = 0.0f;

	if (IsInfiniteLooping())
	{
		// 무한 루프는 종료 상태로 가지 않고 다음 loop-local time 구간을 즉시 준비합니다.
		EmitterTime = 0.0f;
		ResetBurstFiredState();

		const UParticleModuleRequired* RequiredModule = GetRequiredModule();
		if (RequiredModule != nullptr && RequiredModule->bResetSeedOnEmitterLoop)
		{
			RandomStream.Reset();
		}
		return;
	}

	const int32 TotalLoopCount = GetTotalLoopCount();
	if (TotalLoopCount <= 0 || CompletedLoopCount >= TotalLoopCount)
	{
		bEmitterSpawnComplete = true;
		EmitterTime = GetEmitterDuration();
		return;
	}

	EmitterTime = 0.0f;
	ResetBurstFiredState();

	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	if (RequiredModule != nullptr && RequiredModule->bResetSeedOnEmitterLoop)
	{
		RandomStream.Reset();
	}
}

void FParticleEmitterInstance::TickEmitterSpawn(float DeltaTime)
{
	if (DeltaTime <= 0.0f || !CanSpawnEmitter())
	{
		return;
	}

	float RemainingDeltaTime = DeltaTime;
	constexpr float MinSegmentTime = 0.000001f;

	while (RemainingDeltaTime > MinSegmentTime && CanSpawnEmitter())
	{
		const float Duration = GetEmitterDuration();
		const float SegmentStartTime = EmitterTime;
		const float TimeToLoopEnd = Duration - SegmentStartTime;
		if (TimeToLoopEnd <= MinSegmentTime)
		{
			CompleteEmitterLoop();
			continue;
		}

		const float SegmentDeltaTime = std::min(RemainingDeltaTime, TimeToLoopEnd);
		const float SegmentEndTime = SegmentStartTime + SegmentDeltaTime;

		// 한 프레임 안에서 loop boundary를 넘을 수 있으므로, 현재 loop 안에서 처리 가능한 구간만 먼저 spawn 계산
		TickEmitterSpawnSegment(SegmentStartTime, SegmentEndTime);
		EmitterTime = SegmentEndTime;
		RemainingDeltaTime -= SegmentDeltaTime;

		if (EmitterTime >= Duration - MinSegmentTime)
		{
			CompleteEmitterLoop();
		}
	}
}

void FParticleEmitterInstance::TickEmitterSpawnSegment(float SegmentStartTime, float SegmentEndTime)
{
	const float SegmentDeltaTime = std::max(SegmentEndTime - SegmentStartTime, 0.0f);
	if (SegmentDeltaTime <= 0.0f)
	{
		return;
	}

	const int32 BurstSpawnCount = CalculateBurstSpawnCount(SegmentStartTime, SegmentEndTime);
	SpawnParticles(BurstSpawnCount, SegmentStartTime, SegmentDeltaTime);

	SpawnParticles(CalculateSpawnRateCount(SegmentDeltaTime), SegmentStartTime, SegmentDeltaTime);
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
	if (CurrentRuntimeCache == nullptr || CurrentRuntimeCache->SpawnModule == nullptr)
	{
		return 0;
	}

	const UParticleModuleSpawn* SpawnModule = CurrentRuntimeCache->SpawnModule;
	if (!SpawnModule->bProcessBurst || SpawnModule->BurstList.empty())
	{
		return 0;
	}

	if (BurstFiredThisLoop.size() != SpawnModule->BurstList.size())
	{
		ResetBurstFiredState();
	}

	int32 SpawnCount = 0;
	for (int32 BurstIndex = 0; BurstIndex < static_cast<int32>(SpawnModule->BurstList.size()); ++BurstIndex)
	{
		if (BurstFiredThisLoop[static_cast<size_t>(BurstIndex)] != 0u)
		{
			continue;
		}

		const FParticleBurstEntry& Entry = SpawnModule->BurstList[static_cast<size_t>(BurstIndex)];
		if (!Entry.bEnabled)
		{
			continue;
		}

		const float EntryTime = std::max(Entry.Time, 0.0f);
		if (PreviousEmitterTime <= EntryTime && CurrentEmitterTime >= EntryTime)
		{
			BurstFiredThisLoop[static_cast<size_t>(BurstIndex)] = 1u;

			const float Chance = std::clamp(Entry.Chance, 0.0f, 1.0f);
			if (Chance <= 0.0f || (Chance < 1.0f && RandomStream.GetFraction() > Chance))
			{
				continue;
			}

			SpawnCount += ResolveBurstSpawnAmount(Entry);
		}
	}

	return SpawnCount;
}

int32 FParticleEmitterInstance::ResolveBurstSpawnAmount(const FParticleBurstEntry& Entry)
{
	const int32 MaxCount = std::max(Entry.Count, 0);
	if (MaxCount <= 0)
	{
		return 0;
	}

	const int32 MinCount = std::clamp(Entry.CountLow, 0, MaxCount);
	if (MinCount == MaxCount)
	{
		return MaxCount;
	}

	return RandomStream.GetRange(MinCount, MaxCount);
}

int32 FParticleEmitterInstance::SpawnParticles(
	int32 Count,
	float SegmentStartTime,
	float SegmentDeltaTime)
{
	if (Count <= 0 || ParticleData == nullptr || ParticleIndices == nullptr)
	{
		return 0;
	}

	const int32 CurrentLODParticleLimit = std::min(MaxActiveParticles, GetCurrentLODMaxParticles());
	const int32 AvailableSlots = CurrentLODParticleLimit - ActiveParticles;
	if (AvailableSlots <= 0)
	{
		return 0;
	}

	// lower LOD MaxParticles soft cap
	const int32 SpawnCount = std::min(Count, AvailableSlots);
	if (SpawnCount <= 0)
	{
		return 0;
	}

	const float SpawnInterval = SpawnCount > 0 ? SegmentDeltaTime / static_cast<float>(SpawnCount) : 0.0f;

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		const int32 ActiveIndex = ActiveParticles;
		const int32 PhysicalIndex = ParticleIndices[ActiveIndex];
		FBaseParticle& Particle = GetParticleByPhysicalIndex(PhysicalIndex);
		new (&Particle) FBaseParticle();

		ClearParticlePayloads(Particle);

		Particle.Seed = RandomStream.GetUnsignedInt();
		Particle.SpawnId = ++ParticleCounter;
		Particle.OldLocation = Particle.Location;
		Particle.RelativeTime = 0.0f;
		Particle.AgeSeconds = 0.0f;
		Particle.Lifetime = std::max(Particle.Lifetime, 0.0001f);
		Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
		InitializeModulePayloads(Particle);

		const float SpawnTime = SegmentStartTime + SpawnInterval * static_cast<float>(SpawnIndex);
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
		Particle.BaseColor = Particle.Color;

		++ActiveParticles;
	}

	LastFrameSpawnedCount += SpawnCount;
	return SpawnCount;
}

int32 FParticleEmitterInstance::SpawnParticleAtLocation(const FVector& WorldLocation, const FVector& SpawnSide, float SpawnTime)
{
	if (ParticleData == nullptr || ParticleIndices == nullptr || CurrentRuntimeCache == nullptr)
	{
		return -1;
	}

	const int32 CurrentLODParticleLimit = std::min(MaxActiveParticles, GetCurrentLODMaxParticles());
	if (ActiveParticles >= CurrentLODParticleLimit)
	{
		return -1;
	}

	const int32 ActiveIndex = ActiveParticles;
	const int32 PhysicalIndex = ParticleIndices[ActiveIndex];
	FBaseParticle& Particle = GetParticleByPhysicalIndex(PhysicalIndex);
	new (&Particle) FBaseParticle();

	ClearParticlePayloads(Particle);

	Particle.Seed = RandomStream.GetUnsignedInt();
	Particle.SpawnId = ++ParticleCounter;
	Particle.Location = WorldLocation;
	Particle.OldLocation = WorldLocation;
	Particle.RelativeTime = 0.0f;
	Particle.AgeSeconds = 0.0f;
	Particle.Lifetime = std::max(Particle.Lifetime, 0.0001f);
	Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
	InitializeModulePayloads(Particle);

	for (UParticleModule* Module : CurrentRuntimeCache->SpawnModules)
	{
		if (Module == nullptr || !Module->bEnabled)
		{
			continue;
		}

		const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(Module);
		Module->Spawn(this, Offset, SpawnTime, Particle);
	}

	Particle.Location = WorldLocation;
	Particle.OldLocation = WorldLocation;
	Particle.Velocity = FVector::ZeroVector;
	Particle.BaseVelocity = FVector::ZeroVector;
	Particle.Lifetime = std::max(Particle.Lifetime, 0.0001f);
	Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
	Particle.BaseColor = Particle.Color;

	if (CurrentRuntimeCache->TypeDataModule != nullptr)
	{
		const int32 PayloadOffset = CurrentRuntimeCache->GetParticlePayloadOffset(CurrentRuntimeCache->TypeDataModule);
		FRibbonParticlePayload* RibbonPayload = reinterpret_cast<FRibbonParticlePayload*>(GetParticlePayloadByOffset(Particle, PayloadOffset));
		if (RibbonPayload != nullptr)
		{
			RibbonPayload->SpawnSide = SpawnSide.GetSafeNormal();
			if (RibbonPayload->SpawnSide.IsNearlyZero())
			{
				RibbonPayload->SpawnSide = FVector::RightVector;
			}
		}
	}

	++ActiveParticles;
	return PhysicalIndex;
}

bool FParticleEmitterInstance::IsParticlePendingKill(const FBaseParticle& Particle) const
{
	return HasParticleFlag(Particle, EParticleFlags::PendingKill);
}

int32 FParticleEmitterInstance::GetPhysicalIndexByActiveIndex(int32 ActiveIndex) const
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || ParticleIndices == nullptr)
	{
		return -1;
	}

	return static_cast<int32>(ParticleIndices[ActiveIndex]);
}

bool FParticleEmitterInstance::KillParticleByActiveIndex(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return false;
	}

	FBaseParticle& Particle = GetParticleByActiveIndex(ActiveIndex);
	if (IsParticlePendingKill(Particle))
	{
		return false;
	}

	MarkParticlePendingKill(ActiveIndex);
	return true;
}

void FParticleEmitterInstance::MarkParticlePendingKill(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || ParticleIndices == nullptr)
	{
		return;
	}

	const uint16 KilledPhysicalIndex = ParticleIndices[ActiveIndex];
	FBaseParticle& Particle = GetParticleByPhysicalIndex(KilledPhysicalIndex);
	if (IsParticlePendingKill(Particle))
	{
		return;
	}

	SetParticleFlag(Particle, EParticleFlags::PendingKill);
	++LastFrameKilledCount;
}

void FParticleEmitterInstance::CompactPendingKilledParticles()
{
	if (ActiveParticles <= 0 || ParticleIndices == nullptr)
	{
		return;
	}

	TArray<uint16> PendingKilledIndices;
	PendingKilledIndices.reserve(static_cast<size_t>(ActiveParticles));

	int32 LiveParticleCount = 0;
	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
	{
		const uint16 PhysicalIndex = ParticleIndices[ActiveIndex];
		const FBaseParticle& Particle = GetParticleByPhysicalIndex(PhysicalIndex);
		if (IsParticlePendingKill(Particle))
		{
			PendingKilledIndices.push_back(PhysicalIndex);
			continue;
		}

		ParticleIndices[LiveParticleCount] = PhysicalIndex;
		++LiveParticleCount;
	}

	// particle memory는 그대로 두고, active index 목록에서만 죽을 particle을 뒤쪽 free slot 구간으로 밀어냄
	for (int32 PendingIndex = 0; PendingIndex < static_cast<int32>(PendingKilledIndices.size()); ++PendingIndex)
	{
		ParticleIndices[LiveParticleCount + PendingIndex] = PendingKilledIndices[PendingIndex];
	}

	ActiveParticles = LiveParticleCount;
}

void FParticleEmitterInstance::AgeParticles(float DeltaTime)
{
	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
	{
		FBaseParticle& Particle = GetParticleByActiveIndex(ActiveIndex);
		if (IsParticlePendingKill(Particle))
		{
			continue;
		}

		// RelativeTime은 lifetime 진행률, AgeSeconds는 충돌 지연에 쓰는 실제 경과 초
		Particle.AgeSeconds += DeltaTime;
		Particle.RelativeTime += DeltaTime * Particle.OneOverMaxLifetime;

		if (Particle.RelativeTime >= 1.0f)
		{
			MarkParticlePendingKill(ActiveIndex);
		}
	}
}

void FParticleEmitterInstance::UpdateModules(float DeltaTime)
{
	if (CurrentRuntimeCache == nullptr)
	{
		return;
	}

	for (UParticleModule* Module : CurrentRuntimeCache->UpdateModules)
	{
		if (Module != nullptr && Module->bEnabled)
		{
			const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(Module);
			Module->Update(this, Offset, DeltaTime);
		}
	}
}

void FParticleEmitterInstance::IntegrateParticles(float DeltaTime)
{
	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
	{
		FBaseParticle& Particle = GetParticleByActiveIndex(ActiveIndex);
		if (IsParticlePendingKill(Particle))
		{
			continue;
		}

		Particle.OldLocation = Particle.Location;

		const bool bFreezeMovement =
			HasParticleFlag(Particle, EParticleFlags::Freeze) ||
			HasParticleFlag(Particle, EParticleFlags::FreezeMovement);
		if (!bFreezeMovement && !HasParticleFlag(Particle, EParticleFlags::FreezeTranslation))
		{
			Particle.Location += Particle.Velocity * DeltaTime;
		}
		if (!bFreezeMovement && !HasParticleFlag(Particle, EParticleFlags::FreezeRotation))
		{
			Particle.Rotation += Particle.RotationRate * DeltaTime;
		}
	}
}

void FParticleEmitterInstance::UpdateCollisionModules(float DeltaTime)
{
	if (CurrentRuntimeCache == nullptr)
	{
		return;
	}

	for (UParticleModuleCollision* CollisionModule : CurrentRuntimeCache->CollisionModules)
	{
		if (CollisionModule != nullptr && CollisionModule->bEnabled)
		{
			const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(CollisionModule);
			CollisionModule->Update(this, Offset, DeltaTime);
		}
	}
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

void FParticleEmitterInstance::InitializeModulePayloads(FBaseParticle& Particle)
{
	if (CurrentRuntimeCache == nullptr)
	{
		return;
	}

	TArray<UParticleModule*> InitializedModules;
	const auto InitializeModule = [this, &Particle, &InitializedModules](UParticleModule* Module)
	{
		if (Module == nullptr)
		{
			return;
		}

		// 같은 모듈이 spawn / update 양쪽 목록에 들어갈 수 있으므로 중복 검사를 통해 한 번만 초기화
		if (std::find(InitializedModules.begin(), InitializedModules.end(), Module) != InitializedModules.end())
		{
			return;
		}

		InitializedModules.push_back(Module);
		const int32 Offset = CurrentRuntimeCache->GetParticlePayloadOffset(Module);
		Module->InitializeParticle(this, Offset, Particle);
	};

	InitializeModule(CurrentRuntimeCache->RequiredModule);
	InitializeModule(CurrentRuntimeCache->SpawnModule);
	InitializeModule(CurrentRuntimeCache->TypeDataModule);
	
	if (CurrentLODLevel != nullptr)
	{
		// disable된 모듈도 payload offset은 할당되어 있을 수 있으므로, enabled 여부와 관계 없이 모두 초기화 시도
		for (UParticleModule* Module : CurrentLODLevel->Modules)
		{
			InitializeModule(Module);
		}
	}
}

void FParticleEmitterInstance::ClearParticlePayloads(FBaseParticle& Particle) const
{
	if (PayloadOffset < 0 || ParticleStride <= PayloadOffset)
	{
		return;
	}

	std::memset(
		reinterpret_cast<uint8*>(&Particle) + PayloadOffset,
		0,
		static_cast<size_t>(ParticleStride - PayloadOffset));
}

bool FParticleEmitterInstance::UsesLocalSpace() const
{
	return CurrentRuntimeCache == nullptr ||
		CurrentRuntimeCache->RequiredModule == nullptr ||
		CurrentRuntimeCache->RequiredModule->CoordinateSpace == EParticleCoordinateSpace::Local;
}

FVector FParticleEmitterInstance::TransformLocationToWorldSpace(const FVector& SimulationLocation) const
{
	return UsesLocalSpace()
		? Owner.GetComponentToWorld().TransformPosition(SimulationLocation)
		: SimulationLocation;
}

FVector FParticleEmitterInstance::TransformLocationToSimulationSpace(const FVector& WorldLocation) const
{
	if (!UsesLocalSpace())
	{
		return WorldLocation;
	}

	return Owner.GetComponentToWorld().GetInverse().TransformPosition(WorldLocation);
}

FVector FParticleEmitterInstance::TransformVelocityToWorldSpace(const FVector& SimulationVelocity) const
{
	return UsesLocalSpace()
		? Owner.GetComponentToWorld().TransformVector(SimulationVelocity)
		: SimulationVelocity;
}

FVector FParticleEmitterInstance::TransformVelocityToSimulationSpace(const FVector& WorldVelocity) const
{
	if (!UsesLocalSpace())
	{
		return WorldVelocity;
	}

	return Owner.GetComponentToWorld().GetInverse().TransformVector(WorldVelocity);
}

float FParticleEmitterInstance::TransformRadiusToWorldSpace(float Radius) const
{
	const float SafeRadius = std::max(Radius, 0.0f);
	if (!UsesLocalSpace())
	{
		return SafeRadius;
	}

	const FMatrix& ComponentToWorld = Owner.GetComponentToWorld();
	const float MaxScale = std::max(
		ComponentToWorld.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Size(),
		std::max(
			ComponentToWorld.TransformVector(FVector(0.0f, 1.0f, 0.0f)).Size(),
			ComponentToWorld.TransformVector(FVector(0.0f, 0.0f, 1.0f)).Size()));
	return SafeRadius * MaxScale;
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
	LastFrameSpawnedCount = 0;
	LastFrameKilledCount = 0;

	if (!RefreshCurrentRuntimeCache() || CurrentLODLevel == nullptr || CurrentRuntimeCache == nullptr)
	{
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	SecondsSinceCreation += DeltaTime;

	// 이전 프레임에서 kill 예약된 particle 정리
	CompactPendingKilledParticles();

	const bool bLODEnabled = CurrentLODLevel->bEnabled;
	if (bLODEnabled)
	{
		// enabled LOD 전용 spawn 처리
		TickEmitterSpawn(DeltaTime);
	}

	// LOD 비활성 상태에서도 기존 particle 수명 진행 및 kill 허용(disabled LOD에 대한 부드러운 전환)
	AgeParticles(DeltaTime);

	if (bLODEnabled)
	{
		// enabled LOD 전용 update/integrate 처리
		UpdateModules(DeltaTime);
		IntegrateParticles(DeltaTime);
		UpdateCollisionModules(DeltaTime);
	}

	// 이번 프레임에서 수명이 끝난 particle 정리
	CompactPendingKilledParticles();
}

bool FParticleRibbonEmitterInstance::Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex)
{
	if (!FParticleTrailsEmitterInstance::Init(InTemplate, InLODLevelIndex))
	{
		return false;
	}

	RibbonTypeData = CurrentRuntimeCache != nullptr
		? Cast<UParticleModuleTypeDataRibbon>(CurrentRuntimeCache->TypeDataModule)
		: nullptr;

	Trails.clear();
	Trails.resize(1);
	return true;
}

void FParticleRibbonEmitterInstance::Reset()
{
	FParticleTrailsEmitterInstance::Reset();

	for (FRibbonTrailRuntimeState& Trail : Trails)
	{
		Trail.LastSourcePosition = FVector::ZeroVector;
		Trail.DistanceRemainder = 0.0f;
		Trail.bHasSource = false;
	}

	PendingSpawnSourceStart = FVector::ZeroVector;
	PendingSpawnSourceEnd = FVector::ZeroVector;
	PendingSpawnSourceSide = FVector::RightVector;
	bHasPendingSpawnSource = false;
}

void FParticleRibbonEmitterInstance::Tick(float DeltaTime)
{
	if (!RefreshCurrentRuntimeCache() || CurrentLODLevel == nullptr || CurrentRuntimeCache == nullptr || !CurrentLODLevel->bEnabled)
	{
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	SecondsSinceCreation += DeltaTime;

	CompactPendingKilledParticles();

	if (HasEnabledSpawnModule())
	{
		PrepareSpawnModuleSourceSpan();
		TickEmitterSpawn(DeltaTime);
		FinishSpawnModuleSourceSpan();
	}
	else
	{
		EmitterTime += DeltaTime;
		UpdateRibbonTrail(DeltaTime);
	}

	AgeParticles(DeltaTime);
	UpdateModules(DeltaTime);

	const int32 MaxPoints = RibbonTypeData != nullptr ? std::max(RibbonTypeData->MaxParticleInTrailCount, 2) : 2;
	while (ActiveParticles > MaxPoints)
	{
		MarkParticlePendingKill(0);
		CompactPendingKilledParticles();
	}

	CompactPendingKilledParticles();
}

FVector FParticleRibbonEmitterInstance::GetRibbonSourcePosition() const
{
	return GetOwner().GetComponentToWorld().GetOrigin();
}

FVector FParticleRibbonEmitterInstance::GetRibbonSourceSide() const
{
	FVector SourceSide = GetOwner().GetComponentToWorld().GetUnitAxis(EAxis::Y);
	if (SourceSide.IsNearlyZero())
	{
		SourceSide = FVector::RightVector;
	}
	return SourceSide.GetSafeNormal();
}

bool FParticleRibbonEmitterInstance::HasEnabledSpawnModule() const
{
	const UParticleModuleSpawn* SpawnModule = CurrentRuntimeCache != nullptr
		? CurrentRuntimeCache->SpawnModule
		: nullptr;
	if (SpawnModule == nullptr || !SpawnModule->bEnabled)
	{
		return false;
	}

	const bool bHasSpawnRate = SpawnModule->bProcessSpawnRate &&
		(SpawnModule->SpawnRate * SpawnModule->RateScale) > 0.0f;
	const bool bHasBurst = SpawnModule->bProcessBurst && !SpawnModule->BurstList.empty();
	return bHasSpawnRate || bHasBurst;
}

void FParticleRibbonEmitterInstance::PrepareSpawnModuleSourceSpan()
{
	if (Trails.empty())
	{
		Trails.resize(1);
	}

	FRibbonTrailRuntimeState& Trail = Trails[0];
	const FVector SourcePosition = GetRibbonSourcePosition();
	PendingSpawnSourceStart = Trail.bHasSource ? Trail.LastSourcePosition : SourcePosition;
	PendingSpawnSourceEnd = SourcePosition;
	PendingSpawnSourceSide = GetRibbonSourceSide();
	bHasPendingSpawnSource = true;

	Trail.LastSourcePosition = SourcePosition;
	Trail.DistanceRemainder = 0.0f;
	Trail.bHasSource = true;
}

void FParticleRibbonEmitterInstance::FinishSpawnModuleSourceSpan()
{
	bHasPendingSpawnSource = false;
}

int32 FParticleRibbonEmitterInstance::SpawnParticles(int32 Count, float SegmentStartTime, float SegmentDeltaTime)
{
	if (Count <= 0)
	{
		return 0;
	}

	int32 ActualSpawnCount = 0;
	const float SpawnInterval = Count > 0 ? SegmentDeltaTime / static_cast<float>(Count) : 0.0f;
	for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
	{
		const float SpawnTime = SegmentStartTime + SpawnInterval * static_cast<float>(SpawnIndex);
		const float Alpha = SegmentDeltaTime > 1.0e-6f
			? std::clamp((SpawnTime - SegmentStartTime) / SegmentDeltaTime, 0.0f, 1.0f)
			: 1.0f;
		const FVector SourceStart = bHasPendingSpawnSource ? PendingSpawnSourceStart : GetRibbonSourcePosition();
		const FVector SourceEnd = bHasPendingSpawnSource ? PendingSpawnSourceEnd : SourceStart;
		const FVector SourceSide = bHasPendingSpawnSource ? PendingSpawnSourceSide : GetRibbonSourceSide();
		const FVector SpawnLocation = SourceStart + (SourceEnd - SourceStart) * Alpha;

		if (SpawnParticleAtLocation(SpawnLocation, SourceSide, SpawnTime) >= 0)
		{
			++ActualSpawnCount;
		}
		else
		{
			break;
		}
	}

	return ActualSpawnCount;
}

void FParticleRibbonEmitterInstance::UpdateRibbonTrail(float DeltaTime)
{
	(void)DeltaTime;
	if (RibbonTypeData == nullptr || Trails.empty())
	{
		return;
	}

	const FVector SourcePosition = GetRibbonSourcePosition();
	const FVector SourceSide = GetRibbonSourceSide();
	const float SpawnDistance = std::max(RibbonTypeData->SpawnDistance, 0.1f);
	const int32 MaxPoints = std::max(RibbonTypeData->MaxParticleInTrailCount, 2);

	for (FRibbonTrailRuntimeState& Trail : Trails)
	{
		if (!Trail.bHasSource)
		{
			Trail.LastSourcePosition = SourcePosition;
			Trail.DistanceRemainder = 0.0f;
			Trail.bHasSource = true;

			if (RibbonTypeData->bSpawnInitialPoint)
			{
				SpawnParticleAtLocation(SourcePosition, SourceSide, EmitterTime);
			}
			continue;
		}

		const FVector Delta = SourcePosition - Trail.LastSourcePosition;
		const float Distance = Delta.Size();
		if (Distance <= 1.0e-6f)
		{
			continue;
		}

		Trail.DistanceRemainder += Distance;
		const FVector Direction = Delta / Distance;

		while (Trail.DistanceRemainder >= SpawnDistance)
		{
			Trail.DistanceRemainder -= SpawnDistance;
			const FVector PointPosition = SourcePosition - Direction * Trail.DistanceRemainder;
			SpawnParticleAtLocation(PointPosition, SourceSide, EmitterTime);

			while (ActiveParticles > MaxPoints)
			{
				MarkParticlePendingKill(0);
				CompactPendingKilledParticles();
			}
		}

		Trail.LastSourcePosition = SourcePosition;
	}
}

void FParticleRibbonEmitterInstance::BuildRenderSnapshot(
	TArray<FRibbonRenderPoint>& OutPoints,
	TArray<FRibbonRenderRange>& OutRanges) const
{
	OutPoints.clear();
	OutRanges.clear();

	if (RibbonTypeData == nullptr || ActiveParticles <= 0)
	{
		return;
	}

	FRibbonRenderRange Range;
	Range.PointStart = 0;
	Range.PointCount = 0;

	float U = 0.0f;
	FVector PreviousPosition = FVector::ZeroVector;
	bool bHasPrevious = false;

	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
	{
		const FBaseParticle& Particle = GetParticleByActiveIndex(ActiveIndex);
		if (IsParticlePendingKill(Particle))
		{
			continue;
		}

		FRibbonRenderPoint Point;
		Point.Position = Particle.Location;
		Point.Color = Particle.Color.ToVector4();
		Point.Width = RibbonTypeData->bUseParticleSizeAsWidth
			? std::max(std::fabs(Particle.Size.X), 0.1f)
			: std::max(RibbonTypeData->RibbonWidth, 0.1f);

		if (bHasPrevious && RibbonTypeData->TilingDistance > 1.0e-6f)
		{
			U += FVector::Dist(Point.Position, PreviousPosition) / RibbonTypeData->TilingDistance;
		}
		Point.U = U;

		const int32 PayloadOffset = CurrentRuntimeCache != nullptr && CurrentRuntimeCache->TypeDataModule != nullptr
			? CurrentRuntimeCache->GetParticlePayloadOffset(CurrentRuntimeCache->TypeDataModule)
			: -1;
		const FRibbonParticlePayload* RibbonPayload = PayloadOffset >= 0
			? reinterpret_cast<const FRibbonParticlePayload*>(GetParticlePayloadByOffset(Particle, PayloadOffset))
			: nullptr;
		Point.Side = RibbonPayload != nullptr ? RibbonPayload->SpawnSide.GetSafeNormal() : FVector::RightVector;
		if (Point.Side.IsNearlyZero())
		{
			Point.Side = FVector::RightVector;
		}

		OutPoints.push_back(Point);
		PreviousPosition = Point.Position;
		bHasPrevious = true;
		++Range.PointCount;
	}

	if (Range.PointCount >= 2)
	{
		OutRanges.push_back(Range);
	}
	else
	{
		OutPoints.clear();
	}
}

int32 FParticleRibbonEmitterInstance::GetRibbonPointCount() const
{
	return ActiveParticles;
}

bool FParticleAnimTrailEmitterInstance::Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex)
{
	if (!FParticleTrailsEmitterInstance::Init(InTemplate, InLODLevelIndex))
	{
		return false;
	}

	AnimTrailTypeData = CurrentRuntimeCache != nullptr
		? Cast<UParticleModuleTypeDataAnimTrail>(CurrentRuntimeCache->TypeDataModule)
		: nullptr;
	if (AnimTrailTypeData != nullptr)
	{
		FirstSocketName = AnimTrailTypeData->FirstSocketName;
		SecondSocketName = AnimTrailTypeData->SecondSocketName;
		TrailWidth = std::max(AnimTrailTypeData->Width, 0.1f);
	}
	return true;
}

void FParticleAnimTrailEmitterInstance::Reset()
{
	FParticleTrailsEmitterInstance::Reset();
	bTrailEnabled = false;
}

void FParticleAnimTrailEmitterInstance::BeginTrail()
{
	bTrailEnabled = true;
}

void FParticleAnimTrailEmitterInstance::EndTrail()
{
	bTrailEnabled = false;
}

void FParticleAnimTrailEmitterInstance::SetTrailSourceData(
	const FString& InFirstSocketName,
	const FString& InSecondSocketName,
	float InWidth)
{
	FirstSocketName = InFirstSocketName;
	SecondSocketName = InSecondSocketName;
	TrailWidth = std::max(InWidth, 0.1f);
}
