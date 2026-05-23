#include "Particle/ParticleSystemComponent.h"

#include "GameFramework/World.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleEventManager.h"

class UParticleSystemComponent::FInstanceOwner : public IParticleEmitterInstanceOwner
{
public:
	explicit FInstanceOwner(UParticleSystemComponent* InComponent)
		: Component(InComponent)
	{
	}

	UWorld* GetWorld() const override
	{
		return Component != nullptr ? Component->GetWorld() : nullptr;
	}

	FVector GetWorldLocation() const override
	{
		return Component != nullptr ? Component->GetWorldLocation() : FVector::ZeroVector;
	}

	FMatrix GetComponentToWorld() const override
	{
		return Component != nullptr ? Component->GetWorldMatrix() : FMatrix::Identity;
	}

	void AddSpawnEvent(const FParticleEventSpawnData& Event) override
	{
		if (Component != nullptr)
		{
			Component->SpawnEvents.push_back(Event);
		}
	}

	void AddDeathEvent(const FParticleEventDeathData& Event) override
	{
		if (Component != nullptr)
		{
			Component->DeathEvents.push_back(Event);
		}
	}

	void AddCollisionEvent(const FParticleEventCollideData& Event) override
	{
		if (Component != nullptr)
		{
			Component->CollisionEvents.push_back(Event);
		}
	}

	void AddBurstEvent(const FParticleEventBurstData& Event) override
	{
		if (Component != nullptr)
		{
			Component->BurstEvents.push_back(Event);
		}
	}

private:
	UParticleSystemComponent* Component = nullptr;
};

UParticleSystemComponent::UParticleSystemComponent()
{
	bEnableCull = false;
	InstanceOwner = std::make_unique<FInstanceOwner>(this);
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
			? TypeData->CreateInstance(EmitterTemplate, *InstanceOwner)
			: new FParticleEmitterInstance(*InstanceOwner);

		if (Instance != nullptr)
		{
			Instance->SpriteTemplate = EmitterTemplate;
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
