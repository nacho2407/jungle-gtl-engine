#include "Particle/ParticleModules.h"

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Core/ResourceManager.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}

	int32 AlignParticleBytes(int32 Value)
	{
		return ParticleHelper::AlignParticleSize(Value);
	}

	void AddParticlePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutParticleBytes)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytes(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutParticleBytes = AlignParticleBytes(InOutParticleBytes);
		Cache.ModulePayloadOffsets[Module] = InOutParticleBytes;
		InOutParticleBytes += RequiredBytes;
	}

	void AddInstancePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutInstancePayloadSize)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytesPerInstance(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutInstancePayloadSize = AlignParticleBytes(InOutInstancePayloadSize);
		Cache.ModuleInstanceOffsets[Module] = InOutInstancePayloadSize;
		InOutInstancePayloadSize += RequiredBytes;
	}

	FParticleLODLevelRuntimeCache BuildLODLevelRuntimeCache(const UParticleLODLevel* LODLevel)
	{
		FParticleLODLevelRuntimeCache Cache;
		Cache.PayloadOffset = AlignParticleBytes(static_cast<int32>(sizeof(FBaseParticle)));

		int32 ParticleBytes = Cache.PayloadOffset;
		int32 InstancePayloadSize = 0;

		if (LODLevel == nullptr)
		{
			Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
			Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
			return Cache;
		}

		Cache.RequiredModule = LODLevel->RequiredModule;
		Cache.SpawnModule = LODLevel->SpawnModule;
		Cache.TypeDataModule = LODLevel->TypeDataModule;

		if (!LODLevel->bEnabled)
		{
			Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
			Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
			return Cache;
		}

		UParticleModuleTypeDataBase* TypeData = Cache.TypeDataModule;
		if (TypeData != nullptr && TypeData->bEnabled)
		{
			const int32 TypeDataPayloadSize = TypeData->GetRequiredPayloadSize();
			if (TypeDataPayloadSize > 0)
			{
				ParticleBytes = AlignParticleBytes(ParticleBytes);
				ParticleBytes += TypeDataPayloadSize;
			}

			AddInstancePayloadOffset(Cache, TypeData, TypeData, InstancePayloadSize);
		}

		if (Cache.SpawnModule == nullptr)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				if (Module != nullptr && Module->bEnabled && Module->IsSpawnRateModule())
				{
					Cache.SpawnModule = Cast<UParticleModuleSpawn>(Module);
					break;
				}
			}
		}

		// Required / SpawnModule은 Modules 배열과 별개의 특수 모듈이므로 먼저 offset만 계산
		if (Cache.RequiredModule != nullptr && Cache.RequiredModule->bEnabled)
		{
			AddParticlePayloadOffset(Cache, Cache.RequiredModule, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Cache.RequiredModule, TypeData, InstancePayloadSize);
		}
		if (Cache.SpawnModule != nullptr && Cache.SpawnModule->bEnabled)
		{
			AddParticlePayloadOffset(Cache, Cache.SpawnModule, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Cache.SpawnModule, TypeData, InstancePayloadSize);
		}

		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module == nullptr || !Module->bEnabled || Module == Cache.RequiredModule || Module == Cache.SpawnModule || Module == Cache.TypeDataModule)
			{
				continue;
			}

			if (Module->IsSpawnModule())
			{
				Cache.SpawnModules.push_back(Module);
			}

			if (Module->IsUpdateModule())
			{
				Cache.UpdateModules.push_back(Module);
			}

			AddParticlePayloadOffset(Cache, Module, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Module, TypeData, InstancePayloadSize);
		}

		Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
		Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
		return Cache;
	}

	FParticleDistributionContext MakeDistributionContext(
		FParticleEmitterInstance* Owner,
		float SpawnTime,
		const FBaseParticle& Particle)
	{
		FParticleDistributionContext Context;
		Context.RandomStream = Owner != nullptr ? &Owner->RandomStream : nullptr;
		Context.RelativeTime = Particle.RelativeTime;
		Context.SpawnTime = SpawnTime;
		Context.CurveTime = Particle.RelativeTime;
		Context.EmitterTime = Owner != nullptr ? Owner->EmitterTime : 0.0f;
		return Context;
	}

	// TODO: Owned Particle Data 부모 클래스로 빼기
	uint8* GetAlignedSnapshotParticleData(FDynamicSpriteEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}

	uint8* GetAlignedSnapshotParticleData(FDynamicMeshEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}

	FVector GetParticleOldLocationForRender(const FParticleEmitterInstance& EmitterInstance, const FBaseParticle& Particle)
	{
		return EmitterInstance.UsesLocalSpace()
			? EmitterInstance.GetOwner().GetComponentToWorld().TransformPosition(Particle.OldLocation)
			: Particle.OldLocation;
	}
} // namespace

namespace
{
	float GetDistributionEvalTime(const FParticleDistributionContext& Context)
	{
		return Context.CurveTime;
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

	UCurveColorAsset* ResolveColorCurve(const TSoftObjectPtr<UCurveColorAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadColorCurve(Path);
	}

	float EvaluateFloatCurveOrFallback(const TSoftObjectPtr<UCurveFloatAsset>& Curve, float Time, float Fallback)
	{
		UCurveFloatAsset* CurveAsset = ResolveFloatCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}

	FVector EvaluateVectorCurveOrFallback(const TSoftObjectPtr<UCurveVectorAsset>& Curve, float Time, const FVector& Fallback)
	{
		UCurveVectorAsset* CurveAsset = ResolveVectorCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}

	FColor EvaluateColorCurveOrFallback(const TSoftObjectPtr<UCurveColorAsset>& Curve, float Time, const FColor& Fallback)
	{
		UCurveColorAsset* CurveAsset = ResolveColorCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}
}

float EvaluateParticleFloat(const FParticleFloatDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		return Context.RandomStream
			? Context.RandomStream->GetRange(Distribution.Min, Distribution.Max)
			: Distribution.Min;
	case EParticleDistributionMode::Curve:
		return EvaluateFloatCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const float MinValue = EvaluateFloatCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const float MaxValue = EvaluateFloatCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		return Context.RandomStream
			? Context.RandomStream->GetRange(MinValue, MaxValue)
			: MinValue;
	}
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

FVector EvaluateParticleVector(const FParticleVectorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		return Context.RandomStream
			? FVector(
				Context.RandomStream->GetRange(Distribution.Min.X, Distribution.Max.X),
				Context.RandomStream->GetRange(Distribution.Min.Y, Distribution.Max.Y),
				Context.RandomStream->GetRange(Distribution.Min.Z, Distribution.Max.Z))
			: Distribution.Min;
	case EParticleDistributionMode::Curve:
		return EvaluateVectorCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const FVector MinValue = EvaluateVectorCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const FVector MaxValue = EvaluateVectorCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		const float Alpha = Context.RandomStream ? Context.RandomStream->GetFraction() : 0.0f;
		return FVector::Lerp(MinValue, MaxValue, Alpha);
	}
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

FColor EvaluateParticleColor(const FParticleColorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		return Context.RandomStream
			? FColor(
				Context.RandomStream->GetRange(Distribution.Min.R, Distribution.Max.R),
				Context.RandomStream->GetRange(Distribution.Min.G, Distribution.Max.G),
				Context.RandomStream->GetRange(Distribution.Min.B, Distribution.Max.B),
				Context.RandomStream->GetRange(Distribution.Min.A, Distribution.Max.A))
			: Distribution.Min;
	case EParticleDistributionMode::Curve:
		return EvaluateColorCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const FColor MinValue = EvaluateColorCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const FColor MaxValue = EvaluateColorCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		const float Alpha = Context.RandomStream ? Context.RandomStream->GetFraction() : 0.0f;
		return FColor::Lerp(MinValue, MaxValue, Alpha);
	}
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

int32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

int32 UParticleModule::RequiredBytesPerInstance(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

bool UParticleModule::IsSpawnRateModule() const
{
	return false;
}

bool UParticleModule::IsSpawnModule() const
{
	return false;
}

bool UParticleModule::IsUpdateModule() const
{
	return false;
}

void UParticleModule::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	(void)Owner;
	(void)Offset;
	(void)Particle;
}

void UParticleModule::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Owner;
	(void)Offset;
	(void)SpawnTime;
	(void)Particle;
}

void UParticleModule::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)Owner;
	(void)Offset;
	(void)DeltaTime;
}

bool UParticleModuleSpawn::IsSpawnRateModule() const
{
	return true;
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
	Lifetime.Constant = 1.0f;
	Lifetime.Min = 1.0f;
	Lifetime.Max = 1.0f;
}

bool UParticleModuleLifetime::IsSpawnModule() const
{
	return true;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Offset;
	const FParticleDistributionContext Context = MakeDistributionContext(Owner, SpawnTime, Particle);
	Particle.Lifetime = std::max(EvaluateParticleFloat(Lifetime, Context), 0.0001f);
	Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
	Particle.RelativeTime = 0.0f;
}

bool UParticleModuleLocation::IsSpawnModule() const
{
	return true;
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Offset;
	const FParticleDistributionContext Context = MakeDistributionContext(Owner, SpawnTime, Particle);

	// StartLocation은 RequiredModule의 local / world 정책에 맞는 simulation space 값으로 저장
	Particle.Location = EvaluateParticleVector(StartLocation, Context);
	Particle.OldLocation = Particle.Location;
}

bool UParticleModuleVelocity::IsSpawnModule() const
{
	return true;
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Offset;
	const FParticleDistributionContext Context = MakeDistributionContext(Owner, SpawnTime, Particle);
	Particle.Velocity = EvaluateParticleVector(StartVelocity, Context);
	Particle.BaseVelocity = Particle.Velocity;
}

UParticleModuleColor::UParticleModuleColor()
{
	StartColor.Constant = FColor::White();
	StartColor.Min = FColor::White();
	StartColor.Max = FColor::White();
}

bool UParticleModuleColor::IsSpawnModule() const
{
	return true;
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Offset;
	const FParticleDistributionContext Context = MakeDistributionContext(Owner, SpawnTime, Particle);
	Particle.Color = EvaluateParticleColor(StartColor, Context);
	Particle.BaseColor = Particle.Color;
}

UParticleModuleSize::UParticleModuleSize()
{
	StartSize.Constant = FVector::OneVector;
	StartSize.Min = FVector::OneVector;
	StartSize.Max = FVector::OneVector;
}

bool UParticleModuleSize::IsSpawnModule() const
{
	return true;
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Offset;
	const FParticleDistributionContext Context = MakeDistributionContext(Owner, SpawnTime, Particle);
	Particle.Size = EvaluateParticleVector(StartSize, Context);
	Particle.BaseSize = Particle.Size;
}

bool UParticleModuleCollision::IsUpdateModule() const
{
	return true;
}

UParticleLODLevel::~UParticleLODLevel()
{
	if (IsLiveObject(RequiredModule))
	{
		UObjectManager::Get().DestroyObject(RequiredModule);
	}
	RequiredModule = nullptr;

	if (IsLiveObject(SpawnModule))
	{
		UObjectManager::Get().DestroyObject(SpawnModule);
	}
	SpawnModule = nullptr;

	for (UParticleModule* Module : Modules)
	{
		if (Module != RequiredModule && Module != SpawnModule && Module != TypeDataModule && IsLiveObject(Module))
		{
			UObjectManager::Get().DestroyObject(Module);
		}
	}
	Modules.clear();

	if (IsLiveObject(TypeDataModule))
	{
		UObjectManager::Get().DestroyObject(TypeDataModule);
	}
	TypeDataModule = nullptr;
}

void UParticleLODLevel::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleLODLevel* SourceLOD = Cast<UParticleLODLevel>(Original);
	if (!SourceLOD)
	{
		return;
	}

	FDuplicateContext DuplicateContext;
	DuplicateContext.Add(SourceLOD, this);

	RequiredModule = SourceLOD->RequiredModule
		? Cast<UParticleModuleRequired>(SourceLOD->RequiredModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (RequiredModule)
	{
		DuplicateContext.Add(SourceLOD->RequiredModule, RequiredModule);
	}

	SpawnModule = SourceLOD->SpawnModule
		? Cast<UParticleModuleSpawn>(SourceLOD->SpawnModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (SpawnModule)
	{
		DuplicateContext.Add(SourceLOD->SpawnModule, SpawnModule);
	}

	TypeDataModule = SourceLOD->TypeDataModule
		? Cast<UParticleModuleTypeDataBase>(SourceLOD->TypeDataModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (TypeDataModule)
	{
		DuplicateContext.Add(SourceLOD->TypeDataModule, TypeDataModule);
	}

	Modules.clear();
	for (UParticleModule* SourceModule : SourceLOD->Modules)
	{
		UParticleModule* DuplicatedModule = SourceModule
			? Cast<UParticleModule>(SourceModule->Duplicate(&DuplicateContext))
			: nullptr;
		if (DuplicatedModule)
		{
			Modules.push_back(DuplicatedModule);
		}
	}
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (IsLiveObject(LODLevel))
		{
			UObjectManager::Get().DestroyObject(LODLevel);
		}
	}
	LODLevels.clear();
}

void UParticleEmitter::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleEmitter* SourceEmitter = Cast<UParticleEmitter>(Original);
	if (!SourceEmitter)
	{
		return;
	}

	LODLevels.clear();
	for (UParticleLODLevel* SourceLOD : SourceEmitter->LODLevels)
	{
		UParticleLODLevel* DuplicatedLOD = SourceLOD
			? Cast<UParticleLODLevel>(SourceLOD->Duplicate())
			: nullptr;
		if (DuplicatedLOD)
		{
			LODLevels.push_back(DuplicatedLOD);
		}
	}
	CacheEmitterModuleInfo();
}

UParticleSystem::UParticleSystem()
{
	// LOD 0은 항상 0.0f로 설정되어야 함
	LODDistances.push_back(0.0f);
}

UParticleSystem::~UParticleSystem()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (IsLiveObject(Emitter))
		{
			UObjectManager::Get().DestroyObject(Emitter);
		}
	}
	Emitters.clear();
}

void UParticleSystem::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleSystem* SourceParticleSystem = Cast<UParticleSystem>(Original);
	if (!SourceParticleSystem)
	{
		return;
	}

	AssetPath = SourceParticleSystem->AssetPath;
	Emitters.clear();
	for (UParticleEmitter* SourceEmitter : SourceParticleSystem->Emitters)
	{
		UParticleEmitter* DuplicatedEmitter = SourceEmitter
			? Cast<UParticleEmitter>(SourceEmitter->Duplicate())
			: nullptr;
		if (DuplicatedEmitter)
		{
			Emitters.push_back(DuplicatedEmitter);
		}
	}
}

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBase::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	/** TypeDataBase에서는 Sprite용 Render Data임에 유의. 다른 렌더러는 별도의 Render Data가 필요*/

	// EmitterInstance 유효성 체크
	if (InEmitterInstance == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	// Particle Count & Bytes
	const int32 ActiveParticleCount = InEmitterInstance->ActiveParticles;
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	const size_t ParticleDataBytes =
		static_cast<size_t>(ActiveParticleCount) * static_cast<size_t>(ParticleStride);
	const size_t ParticleIndexBytes =
		static_cast<size_t>(ActiveParticleCount) * sizeof(uint16);
	const size_t SnapshotLogicalBytes = ParticleDataBytes + ParticleIndexBytes;
	if (ParticleDataBytes > static_cast<size_t>(std::numeric_limits<int32>::max()) ||
		SnapshotLogicalBytes > static_cast<size_t>(std::numeric_limits<int32>::max()))
	{
		return nullptr;
	}

	FDynamicSpriteEmitterData* RenderData = new FDynamicSpriteEmitterData();
	RenderData->OwnedParticleData.resize(ParticleDataBytes + ParticleHelper::ParticleAlignment);
	RenderData->OwnedParticleIndices.resize(static_cast<size_t>(ActiveParticleCount));

	uint8* SnapshotParticleData = GetAlignedSnapshotParticleData(*RenderData);
	if (SnapshotParticleData == nullptr)
	{
		delete RenderData;
		return nullptr;
	}

	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticleCount; ++ActiveIndex)
	{
		const int32 SourcePhysicalIndex = InEmitterInstance->ParticleIndices[ActiveIndex];
		const uint8* SourceParticleData =
			InEmitterInstance->ParticleData + static_cast<size_t>(SourcePhysicalIndex) * ParticleStride;
		uint8* DestinationParticleData = SnapshotParticleData + static_cast<size_t>(ActiveIndex) * ParticleStride;

		// renderer가 simulation memory를 직접 읽지 않아도 되도록 payload까지 포함한 particle stride 전체를 복사
		std::memcpy(DestinationParticleData, SourceParticleData, static_cast<size_t>(ParticleStride));

		const FBaseParticle& SourceParticle = *reinterpret_cast<const FBaseParticle*>(SourceParticleData);
		FBaseParticle& SnapshotParticle = *reinterpret_cast<FBaseParticle*>(DestinationParticleData);
		SnapshotParticle.Location = InEmitterInstance->GetParticleLocationForRender(SourceParticle);
		SnapshotParticle.OldLocation = GetParticleOldLocationForRender(*InEmitterInstance, SourceParticle);
		RenderData->OwnedParticleIndices[ActiveIndex] = static_cast<uint16>(ActiveIndex);
	}

	UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;
	RenderData->ReplayData.eEmitterType = EDynamicEmitterType::Sprite;
	RenderData->ReplayData.ActiveParticleCount = ActiveParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;
	RenderData->Material = RequiredModule != nullptr ? RequiredModule->Material : nullptr;

	// snapshot의 Location / OldLocation은 이미 world space이므로 renderer가 component transform을 다시 적용하면 안 됨!
	RenderData->ReplayData.CoordinateSpace = EParticleCoordinateSpace::World;
	RenderData->ComponentToWorld = FMatrix::Identity;
	RenderData->ReplayData.Scale = FVector::OneVector;

	// renderer가 설정을 읽기 위한 단순 참조용 포인터. renderer 쪽에서 수정 금지!
	RenderData->ReplayData.RequiredModule = RequiredModule;

	// snapshot은 particle data와 index를 별도 버퍼로 소유. renderer는 연속 메모리를 가정하지 말고
	// 반드시 DataContainer의 ParticleData / ParticleIndices 포인터를 통해 접근해야 함
	RenderData->ReplayData.DataContainer.MemBlockSize = static_cast<int32>(SnapshotLogicalBytes);
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = static_cast<int32>(ParticleDataBytes);
	RenderData->ReplayData.DataContainer.ParticleIndicesNumShorts = ActiveParticleCount;
	RenderData->ReplayData.DataContainer.ParticleData = SnapshotParticleData;
	RenderData->ReplayData.DataContainer.ParticleIndices = RenderData->OwnedParticleIndices.data();

	return RenderData;
}

int32 UParticleModuleTypeDataBase::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return GetRequiredPayloadSize();
}

int32 UParticleModuleTypeDataBase::GetRequiredPayloadSize() const
{
	return 0;
}

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleMeshEmitterInstance* Instance = new FParticleMeshEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataMesh::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	// EmitterIntsance 유효성 검사
	if (InEmitterInstance == nullptr ||
		GetStaticMesh() == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	// Particle Count
	const int32 ActiveParticleCount = InEmitterInstance->ActiveParticles;
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	const size_t ParticleDataBytes = static_cast<size_t>(ActiveParticleCount) * static_cast<size_t>(ParticleStride);
	const size_t ParticleIndexBytes = static_cast<size_t>(ActiveParticleCount) * sizeof(uint16);
	const size_t SnapshotLogicalBytes = ParticleDataBytes + ParticleIndexBytes;

	// Mesh
	FDynamicMeshEmitterData* RenderData = new FDynamicMeshEmitterData();
	RenderData->Mesh = GetStaticMesh();

	// Particle Data, Indices
	RenderData->OwnedParticleData.resize(ParticleDataBytes + ParticleHelper::ParticleAlignment);
	RenderData->OwnedParticleIndices.resize(static_cast<size_t>(ActiveParticleCount));

	uint8* SnapshotParticleData = GetAlignedSnapshotParticleData(*RenderData);
	if (SnapshotParticleData == nullptr)
	{
		delete RenderData;
		return nullptr;
	}

	// Require Module
	const UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;

	// Sort Mode
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;

	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticleCount; ++ActiveIndex)
	{
		const int32 SourcePhysicalIndex = InEmitterInstance->ParticleIndices[ActiveIndex];
		const uint8* SourceParticleData =
			InEmitterInstance->ParticleData + static_cast<size_t>(SourcePhysicalIndex) * ParticleStride;
		uint8* DestinationParticleData = SnapshotParticleData + static_cast<size_t>(ActiveIndex) * ParticleStride;

		// renderer가 simulation memory를 직접 읽지 않도록 payload까지 포함한 particle stride 전체를 snapshot에 복사
		std::memcpy(DestinationParticleData, SourceParticleData, static_cast<size_t>(ParticleStride));
		RenderData->OwnedParticleIndices[ActiveIndex] = static_cast<uint16>(ActiveIndex);
	}
	// ReplayData
	RenderData->ReplayData.ActiveParticleCount = ActiveParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	RenderData->Material = RequiredModule != nullptr ? RequiredModule->Material : nullptr;

	// renderer가 raw particle의 위치를 render space로 해석할 수 있도록 좌표계 메타데이터를 전달
	RenderData->ComponentToWorld = InEmitterInstance->GetOwner().GetComponentToWorld();
	RenderData->ReplayData.CoordinateSpace = RequiredModule != nullptr
		? RequiredModule->CoordinateSpace
		: EParticleCoordinateSpace::Local;

	// TODO: 중앙 renderer가 Mesh instance transform을 생성할 때 Mesh Particle의 회전 축과 정렬 정책을 반영한다.
	RenderData->ReplayData.DataContainer.MemBlockSize = static_cast<int32>(SnapshotLogicalBytes);
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = static_cast<int32>(ParticleDataBytes);
	RenderData->ReplayData.DataContainer.ParticleIndicesNumShorts = ActiveParticleCount;
	RenderData->ReplayData.DataContainer.ParticleData = SnapshotParticleData;
	RenderData->ReplayData.DataContainer.ParticleIndices = RenderData->OwnedParticleIndices.data();

	return RenderData;
}

void UParticleModuleTypeDataMesh::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	Mesh = InStaticMesh;
	MeshAssetPath.SetPath(Mesh != nullptr ? Mesh->GetAssetPathFileName() : FString());
}

void UParticleModuleTypeDataMesh::PostEditProperty(const char* PropertyName)
{
	UParticleModuleTypeDataBase::PostEditProperty(PropertyName);

	if (PropertyName != nullptr && std::strcmp(PropertyName, "MeshAssetPath") == 0)
	{
		const FString RequestedPath = MeshAssetPath.GetPath();
		SetStaticMesh(RequestedPath.empty() ? nullptr : FResourceManager::Get().LoadStaticMesh(RequestedPath));
	}
}

int32 FParticleLODLevelRuntimeCache::GetParticlePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModulePayloadOffsets.find(Module);
	return It != ModulePayloadOffsets.end() ? It->second : -1;
}

int32 FParticleLODLevelRuntimeCache::GetInstancePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModuleInstanceOffsets.find(Module);
	return It != ModuleInstanceOffsets.end() ? It->second : -1;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	LODLevelRuntimeCaches.clear();
	LODLevelRuntimeCaches.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		FParticleLODLevelRuntimeCache Cache = BuildLODLevelRuntimeCache(LODLevel);
		LODLevelRuntimeCaches.push_back(Cache);
	}
}

TArray<int32> UParticleEmitter::CalculateTotalPayloadSize() const
{
	TArray<int32> Result;
	Result.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		Result.push_back(BuildLODLevelRuntimeCache(LODLevel).ParticleStride);
	}

	return Result;
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex)
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache()
{
	return GetLODLevelRuntimeCache(0);
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache() const
{
	return GetLODLevelRuntimeCache(0);
}
