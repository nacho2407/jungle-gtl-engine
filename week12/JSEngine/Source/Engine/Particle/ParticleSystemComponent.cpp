#include "Particle/ParticleSystemComponent.h"

#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "GameFramework/World.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleEventManager.h"

namespace
{
	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}
}

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
	ReleaseRenderData();
	ReleaseEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (ResolvedTemplate == InTemplate)
	{
		return;
	}

	ResolvedTemplate = InTemplate;
	Template.SetPath(InTemplate ? InTemplate->GetAssetPath() : FString());
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::SetTemplateAsset(const TSoftObjectPtr<UParticleSystem>& InTemplate)
{
	Template = InTemplate;
	ResolvedTemplate = nullptr;
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::SetTemplatePath(const FString& InPath)
{
	Template.SetPath(FPaths::Normalize(InPath));
	ResolvedTemplate = nullptr;
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

UParticleSystem* UParticleSystemComponent::GetTemplate()
{
	if (ResolvedTemplate)
	{
		return ResolvedTemplate;
	}

	const FString Path = FPaths::Normalize(Template.GetPath());
	if (Path.empty())
	{
		return nullptr;
	}

	ResolvedTemplate = FResourceManager::Get().LoadParticleSystem(Path);
	return ResolvedTemplate;
}

const UParticleSystem* UParticleSystemComponent::GetTemplate() const
{
	return const_cast<UParticleSystemComponent*>(this)->GetTemplate();
}

void UParticleSystemComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
		ResolvedTemplate = nullptr;
		ReleaseEmitterInstances();
		CreateEmitterInstances();
	}
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (PropertyName && FString(PropertyName) == "Template")
	{
		ResolvedTemplate = nullptr;
		ReleaseEmitterInstances();
		CreateEmitterInstances();
	}
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
		if (Instance == nullptr || !IsLiveObject(Instance->CurrentLODLevel))
		{
			continue;
		}

		UParticleModuleTypeDataBase* TypeDataModule = Instance->CurrentLODLevel->TypeDataModule;
		if (!IsLiveObject(TypeDataModule))
		{
			continue;
		}

		FDynamicEmitterDataBase* RenderData = TypeDataModule->GetDynamicRenderData(Instance);
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
	UParticleSystem* ParticleSystem = GetTemplate();
	if (ParticleSystem == nullptr)
	{
		return;
	}

	for (UParticleEmitter* EmitterTemplate : ParticleSystem->Emitters)
	{
		if (!IsLiveObject(EmitterTemplate))
		{
			continue;
		}

		EmitterTemplate->CacheEmitterModuleInfo();

		UParticleLODLevel* LODLevel = EmitterTemplate->LODLevels.empty() ? nullptr : EmitterTemplate->LODLevels[0];
		UParticleModuleTypeDataBase* TypeData = IsLiveObject(LODLevel) && IsLiveObject(LODLevel->TypeDataModule)
			? LODLevel->TypeDataModule
			: nullptr;

		if (TypeData == nullptr)
		{
			UE_LOG_WARNING("[Particle] Emitter has no TypeDataModule. Falling back to base particle emitter instance.");
		}

		FParticleEmitterInstance* Instance = TypeData != nullptr
			? TypeData->CreateInstance(EmitterTemplate, *InstanceOwner)
			: new FParticleEmitterInstance(*InstanceOwner);

		if (Instance != nullptr && Instance->Init(EmitterTemplate, 0))
		{
			EmitterInstances.push_back(Instance);
		}
		else
		{
			delete Instance;
		}
	}
}
