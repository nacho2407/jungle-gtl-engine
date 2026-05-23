#include "Particle/ParticleSystemComponent.h"

#include "GameFramework/World.h"

namespace
{
	// 사용되지 않는 선언 컴파일 경고 방지용. 구현 후 삭제할 것
	void ParticleNoOp(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
	{
		(void)Owner;
		(void)Offset;
		(void)DeltaTime;
	}
}

void UParticleModuleRequired::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleRequired::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSpawn::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSpawn::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLifetime::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLocation::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleVelocity::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleTypeDataBase::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleTypeDataBase::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

UParticleLODLevel::~UParticleLODLevel()
{
	UObjectManager::Get().DestroyObject(RequiredModule);
	RequiredModule = nullptr;

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
	ParticleSize.clear();
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
	IParticleEmitterInstanceOwner& InComponent)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
	Instance->SpriteTemplate = InEmitterTemplate;
	Instance->Component = InComponent.GetParticleSystemComponent();
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBase::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	(void)InEmitterInstance;
	return nullptr;
}

int32 UParticleModuleTypeDataBase::GetRequiredPayloadSize() const
{
	return 0;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ParticleSize = CalculateTotalPayloadSize();
}

TArray<int32> UParticleEmitter::CalculateTotalPayloadSize() const
{
	TArray<int32> Result;
	Result.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		int32 PayloadSize = 0;
		if (LODLevel != nullptr && LODLevel->TypeDataModule != nullptr)
		{
			PayloadSize += LODLevel->TypeDataModule->GetRequiredPayloadSize();
		}

		Result.push_back(static_cast<int32>(sizeof(FBaseParticle)) + PayloadSize);
	}

	return Result;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	(void)DeltaTime;

	if (CurrentLODLevel == nullptr || !CurrentLODLevel->bEnabled)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->Modules)
	{
		if (Module != nullptr)
		{
			Module->Update(this, 0, DeltaTime);
		}
	}
}

UParticleSystemComponent::UParticleSystemComponent()
{
	bEnableCull = false;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ReleaseEmitterInstances();
	ReleaseRenderData();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate)
	{
		return;
	}

	Template = InTemplate;
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

UWorld* UParticleSystemComponent::GetWorld() const
{
	return GetOwner() != nullptr ? GetOwner()->GetFocusedWorld() : nullptr;
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance != nullptr)
		{
			Instance->Tick(DeltaTime);
		}
	}

	PackRenderData();
}

void UParticleSystemComponent::FinalizeTickComponent()
{
	if (EventManager != nullptr)
	{
		if (!SpawnEvents.empty()) EventManager->HandleParticleSpawnEvents(this, SpawnEvents);
		if (!DeathEvents.empty()) EventManager->HandleParticleDeathEvents(this, DeathEvents);
		if (!CollisionEvents.empty()) EventManager->HandleParticleCollisionEvents(this, CollisionEvents);
		if (!BurstEvents.empty()) EventManager->HandleParticleBurstEvents(this, BurstEvents);
	}

	SpawnEvents.clear();
	DeathEvents.clear();
	CollisionEvents.clear();
	BurstEvents.clear();
}

void UParticleSystemComponent::PackRenderData()
{
	ReleaseRenderData();

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if (Instance == nullptr || Instance->CurrentLODLevel == nullptr || Instance->CurrentLODLevel->TypeDataModule == nullptr)
		{
			continue;
		}

		FDynamicEmitterDataBase* RenderData = Instance->CurrentLODLevel->TypeDataModule->GetDynamicRenderData(Instance);
		if (RenderData != nullptr)
		{
			RenderData->EmitterIndex = EmitterIndex;
			EmitterRenderData.push_back(RenderData);
		}
	}
}

void UParticleSystemComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	const FVector Center = GetWorldLocation();
	const FVector Extent(1.0f, 1.0f, 1.0f);
	WorldAABB.Expand(Center - Extent);
	WorldAABB.Expand(Center + Extent);
}

bool UParticleSystemComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	(void)Ray;
	(void)OutHitResult;
	return false;
}

void UParticleSystemComponent::ResetParticles()
{
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::ReleaseEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.clear();
}

void UParticleSystemComponent::ReleaseRenderData()
{
	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		delete RenderData;
	}
	EmitterRenderData.clear();
}

void UParticleSystemComponent::CreateEmitterInstances()
{
	if (Template == nullptr)
	{
		return;
	}

	for (UParticleEmitter* EmitterTemplate : Template->Emitters)
	{
		if (EmitterTemplate == nullptr)
		{
			continue;
		}

		EmitterTemplate->CacheEmitterModuleInfo();

		UParticleLODLevel* LODLevel = EmitterTemplate->LODLevels.empty() ? nullptr : EmitterTemplate->LODLevels[0];
		UParticleModuleTypeDataBase* TypeData = LODLevel != nullptr ? LODLevel->TypeDataModule : nullptr;

		FParticleEmitterInstance* Instance = TypeData != nullptr
			? TypeData->CreateInstance(EmitterTemplate, *this)
			: new FParticleEmitterInstance();

		if (Instance != nullptr)
		{
			Instance->SpriteTemplate = EmitterTemplate;
			Instance->Component = this;
			Instance->CurrentLODLevelIndex = 0;
			Instance->CurrentLODLevel = LODLevel;
			Instance->ParticleStride = EmitterTemplate->ParticleSize.empty() ? static_cast<int32>(sizeof(FBaseParticle)) : EmitterTemplate->ParticleSize[0];
			Instance->MaxActiveParticles = LODLevel != nullptr && LODLevel->RequiredModule != nullptr
				? LODLevel->RequiredModule->MaxParticles
				: 0;
			EmitterInstances.push_back(Instance);
		}
	}
}

void AParticleEventManager::HandleParticleSpawnEvents(UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& InSpawnEvents)
{
	(void)Component;
	(void)InSpawnEvents;
}

void AParticleEventManager::HandleParticleDeathEvents(UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& InDeathEvents)
{
	(void)Component;
	(void)InDeathEvents;
}

void AParticleEventManager::HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents)
{
	(void)Component;
	(void)InCollisionEvents;
}

void AParticleEventManager::HandleParticleBurstEvents(UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& InBurstEvents)
{
	(void)Component;
	(void)InBurstEvents;
}
