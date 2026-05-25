#include "Particle/ParticleSystemComponent.h"

#include "Camera/ViewportCamera.h"
#include "GameFramework/World.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleEventManager.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include <algorithm>
#include <cstring>
#include <memory>

// EmitterInstance에서 Component를 직접 참조해서 강하게 결합하는 문제를 해결하기 위해
// Component의 일부 기능만 인터페이스로 제공하는 InstanceOwner를 EmitterInstance에 넘긴다
// 위치를 보고 의문이 들 수 있지만 UE도 여기에 위치함
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

void UParticleSystemComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsLoading())
	{
		ResolveTemplateAssetReference();
	}
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (PropertyName != nullptr && std::strcmp(PropertyName, "TemplateAssetPath") == 0)
	{
		ResolveTemplateAssetReference();
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate)
	{
		return;
	}

	Template = InTemplate;
	// Template 변경 
	// -> 기존 Emitter Instance 모두 해제 
	// -> 새 Template 기준으로 EmitterIntsnace 생성
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::ResolveTemplateAssetReference()
{
	if (TemplateAssetPath.IsNull())
	{
		SetTemplate(nullptr);
		return;
	}

	// TODO: ParticleSystem Asset 로더가 완성되면 TemplateAssetPath 경로로 로드한 템플릿을 SetTemplate에 전달한다.
	SetTemplate(nullptr);
}

UWorld* UParticleSystemComponent::GetWorld() const
{
	return GetOwner() != nullptr ? GetOwner()->GetFocusedWorld() : nullptr;
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	UpdateLODLevel();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance != nullptr)
		{
			Instance->Tick(DeltaTime);
		}
	}

	// Render Data 수집
	PackRenderData();
	
	// Tick이 끝날 때 Event를 처리
    FinalizeTickComponent();
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
	// TODO: 모든 emitter 타입의 bounds snapshot 계약이 정해지면 PSC 단위 경계를 계산한다.
}

bool UParticleSystemComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	(void)Ray;
	(void)OutHitResult;
	// TODO: 모든 emitter 타입의 picking snapshot 계약이 정해지면 PSC 단위 raycast를 구현한다.
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

int32 UParticleSystemComponent::SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate) const
{
	if (EmitterTemplate == nullptr || EmitterTemplate->LODLevels.size() <= 1 || LODDistanceInterval <= 0.0f)
	{
		return 0;
	}

	// 현재 World에 활성화된 Camera 정보를 가져온다
	const UWorld* World = GetWorld();
	const FViewportCamera* ActiveCamera = World != nullptr ? World->GetActiveCamera() : nullptr;
	if (ActiveCamera == nullptr)
	{
		return 0;
	}

	// Camera와 ParicleSystemComponent간 거리 계산
	// 임시로 1000 단위로 LOD 레벨을 확장한다. [0, LODLevels.size())로 범위 제한.
	const float Distance = FVector::Distance(GetWorldLocation(), ActiveCamera->GetLocation());
	const int32 SelectedIndex = static_cast<int32>(Distance / LODDistanceInterval);
	return std::clamp(SelectedIndex, 0, static_cast<int32>(EmitterTemplate->LODLevels.size()) - 1);
}

void UParticleSystemComponent::UpdateLODLevel()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance != nullptr &&
			Instance->SpriteTemplate != nullptr &&
			Instance->CurrentLODLevelIndex != SelectLODLevelIndex(Instance->SpriteTemplate))
		{
			// 선택 LOD가 바뀌면 TypeData 종류와 payload layout도 달라질 수 있으므로 모든 instance를 다시 생성
			ReleaseRenderData();
			ReleaseEmitterInstances();
			CreateEmitterInstances();
			return;
		}
	}
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

		// LOD 기반으로 Particle LOD Level 및 TypeData를 가져온다
		const int32 LODIndex = SelectLODLevelIndex(EmitterTemplate);
		UParticleLODLevel* LODLevel = EmitterTemplate->LODLevels.empty() ? nullptr : EmitterTemplate->LODLevels[LODIndex];
		UParticleModuleTypeDataBase* TypeData = LODLevel != nullptr ? LODLevel->TypeDataModule : nullptr;

		if (TypeData == nullptr)
		{
			UE_LOG_WARNING("[Particle] Emitter has no TypeDataModule. Falling back to base particle emitter instance.");
		}

		FParticleEmitterInstance* Instance = TypeData != nullptr
			? TypeData->CreateInstance(EmitterTemplate, *InstanceOwner)
			: new FParticleEmitterInstance(*InstanceOwner);

		if (Instance != nullptr && Instance->Init(EmitterTemplate, LODIndex))
		{
			EmitterInstances.push_back(Instance);
		}
		else
		{
			delete Instance;
		}
	}
}
