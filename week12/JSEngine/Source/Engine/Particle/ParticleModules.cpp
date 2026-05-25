#include "Particle/ParticleModules.h"

#include "Core/ResourceManager.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"

#include <algorithm>
#include <cstring>

namespace
{
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

		if (Cache.SpawnModule == nullptr)
		{
			for (UParticleModule* Module : LODLevel->Modules)
			{
				if (Module != nullptr && Module->IsSpawnRateModule())
				{
					Cache.SpawnModule = Cast<UParticleModuleSpawn>(Module);
					break;
				}
			}
		}

		// 모든 특수 모듈의 payload offset도 실제 실행 모듈 list와 같은 시작 주소 map에 넣어둠(별도 분리 안함)
		AddParticlePayloadOffset(Cache, Cache.RequiredModule, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, Cache.RequiredModule, TypeData, InstancePayloadSize);
		AddParticlePayloadOffset(Cache, Cache.SpawnModule, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, Cache.SpawnModule, TypeData, InstancePayloadSize);
		AddParticlePayloadOffset(Cache, Cache.TypeDataModule, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, Cache.TypeDataModule, TypeData, InstancePayloadSize);

		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module == nullptr || Module == Cache.RequiredModule || Module == Cache.SpawnModule || Module == Cache.TypeDataModule)
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
		return Context;
	}

	uint8* GetAlignedSnapshotParticleData(FDynamicSpriteEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}
} // namespace

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
	UObjectManager::Get().DestroyObject(RequiredModule);
	RequiredModule = nullptr;

	UObjectManager::Get().DestroyObject(SpawnModule);
	SpawnModule = nullptr;

	for (UParticleModule* Module : Modules)
	{
		UObjectManager::Get().DestroyObject(Module);
	}
	Modules.clear();

	UObjectManager::Get().DestroyObject(TypeDataModule);
	TypeDataModule = nullptr;
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		UObjectManager::Get().DestroyObject(LODLevel);
	}
	LODLevels.clear();
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
		UObjectManager::Get().DestroyObject(Emitter);
	}
	Emitters.clear();
}

void UParticleModuleTypeDataBase::Build()
{
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
    // TypeDataBase에서는 Sprite용 Render Data임에 유의. 다른 렌더러는 별도의 Render Data가 필요

	if (InEmitterInstance == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	const int32 ActiveParticleCount = InEmitterInstance->ActiveParticles;
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	const int32 ParticleDataBytes = ActiveParticleCount * ParticleStride;
	const int32 ParticleIndexBytes = ActiveParticleCount * static_cast<int32>(sizeof(uint16));

	FDynamicSpriteEmitterData* RenderData = new FDynamicSpriteEmitterData();
	RenderData->OwnedParticleData.resize(static_cast<size_t>(ParticleDataBytes + ParticleHelper::ParticleAlignment));
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
		RenderData->OwnedParticleIndices[ActiveIndex] = static_cast<uint16>(ActiveIndex);
	}

	UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;
	RenderData->ReplayData.ActiveParticleCount = ActiveParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;
	RenderData->ReplayData.MaterialInterface = RequiredModule != nullptr ? RequiredModule->Material : nullptr;
	RenderData->ReplayData.RequiredModule = RequiredModule;

	RenderData->ReplayData.DataContainer.MemBlockSize = ParticleDataBytes + ParticleIndexBytes;
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = ParticleDataBytes;
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
	if (InEmitterInstance == nullptr || GetStaticMesh() == nullptr)
	{
		return nullptr;
	}

	// Mesh Emitter Render Data 생성
	FDynamicMeshEmitterData* RenderData = new FDynamicMeshEmitterData();
	RenderData->Mesh = GetStaticMesh();
	RenderData->ReplayData.ParticleStride = static_cast<int32>(sizeof(FMeshParticleInstanceVertex));

	// Runtime Cache 참조해서 RequiredModule 채우기
	const UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache != nullptr
		? InEmitterInstance->CurrentRuntimeCache->RequiredModule
		: nullptr;

	// Sort Mode
	if (RequiredModule != nullptr)
	{
		RenderData->ReplayData.SortMode = RequiredModule->SortMode;
	}

	// Active Particle Count 만큼 공간 할당 후 데이터를 채운다.
	const int32 ActiveParticleCount = InEmitterInstance->GetActiveParticleCount();
	RenderData->InstanceVertices.reserve(ActiveParticleCount);

	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticleCount; ++ActiveIndex)
	{
		const FBaseParticle& Particle = InEmitterInstance->GetParticleByActiveIndex(ActiveIndex);

		FMeshParticleInstanceVertex InstanceVertex;
		InstanceVertex.Transform =
			FMatrix::MakeScale(Particle.Size) *
			FMatrix::MakeTranslation(Particle.Location);

		if (InEmitterInstance->UsesLocalSpace())
		{
			InstanceVertex.Transform *= InEmitterInstance->GetOwner().GetComponentToWorld();
		}

		// TODO: Mesh Particle의 회전 축과 정렬 정책이 합의되면 Particle.Rotation을 Transform에 반영한다.
		InstanceVertex.Color = Particle.Color.ToVector4();
		RenderData->InstanceVertices.push_back(InstanceVertex);
	}

	RenderData->ReplayData.ActiveParticleCount = static_cast<int32>(RenderData->InstanceVertices.size());
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
