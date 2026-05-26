#include "Particle/ParticleSystemComponent.h"

#include "Camera/ViewportCamera.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "GameFramework/World.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleEventManager.h"
#include "Particle/ParticleMeshBounds.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Render/Scene/ParticleSystemRenderProxy.h"
#include <algorithm>
#include <cstring>
#include <memory>

namespace
{
	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}

	bool IsSoloParticleInstance(const FParticleEmitterInstance* Instance)
	{
		return Instance != nullptr &&
			IsLiveObject(Instance->CurrentLODLevel) &&
			Instance->CurrentLODLevel->bSolo;
	}

	UParticleLODLevel* GetLODLevel(UParticleEmitter* EmitterTemplate, int32 LODIndex)
	{
		// template / index 방어
		if (!IsLiveObject(EmitterTemplate) ||
			LODIndex < 0 ||
			LODIndex >= static_cast<int32>(EmitterTemplate->LODLevels.size()))
		{
			return nullptr;
		}

		// live LOD만 반환
		UParticleLODLevel* LODLevel = EmitterTemplate->LODLevels[static_cast<size_t>(LODIndex)];
		return IsLiveObject(LODLevel) ? LODLevel : nullptr;
	}

	UParticleModuleTypeDataBase* GetLiveTypeData(UParticleLODLevel* LODLevel)
	{
		// null TypeData 허용
		return IsLiveObject(LODLevel) && IsLiveObject(LODLevel->TypeDataModule)
			? LODLevel->TypeDataModule
			: nullptr;
	}

	const FDynamicMeshEmitterData* FindMeshRenderDataForEmitter(
		const TArray<FDynamicEmitterDataBase*>& RenderData,
		int32 EmitterIndex)
	{
		for (const FDynamicEmitterDataBase* EmitterData : RenderData)
		{
			if (EmitterData != nullptr &&
				EmitterData->EmitterIndex == EmitterIndex &&
				EmitterData->GetEmitterType() == EDynamicEmitterType::Mesh)
			{
				return static_cast<const FDynamicMeshEmitterData*>(EmitterData);
			}
		}
		return nullptr;
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
			Component->ReportEventSpawn(Event);
		}
	}

	void AddDeathEvent(const FParticleEventDeathData& Event) override
	{
		if (Component != nullptr)
		{
			Component->ReportEventDeath(Event);
		}
	}

	void AddCollisionEvent(const FParticleEventCollideData& Event) override
	{
		if (Component != nullptr)
		{
			Component->ReportEventCollision(Event);
		}
	}

	void AddBurstEvent(const FParticleEventBurstData& Event) override
	{
		if (Component != nullptr)
		{
			Component->ReportEventBurst(Event);
		}
	}

private:
	UParticleSystemComponent* Component = nullptr;
};

UParticleSystemComponent::UParticleSystemComponent()
{
	bEnableCull = false;
	InstanceOwner = std::make_unique<FInstanceOwner>(this);
	RenderProxy = std::make_unique<FParticleSystemRenderProxy>(this);
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ReleaseRenderProxyResources();
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
	ReleaseRenderProxyResources();
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::SetTemplateAsset(const TSoftObjectPtr<UParticleSystem>& InTemplate)
{
	Template = InTemplate;
	ResolvedTemplate = nullptr;
	ReleaseRenderProxyResources();
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
}

void UParticleSystemComponent::SetTemplatePath(const FString& InPath)
{
	Template.SetPath(FPaths::Normalize(InPath));
	ResolvedTemplate = nullptr;
	ReleaseRenderProxyResources();
	ReleaseRenderData();
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
		ReleaseRenderProxyResources();
		ReleaseRenderData();
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
		ReleaseRenderProxyResources();
		ReleaseRenderData();
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
	SpawnEvents.clear();
	DeathEvents.clear();
	CollisionEvents.clear();
	BurstEvents.clear();

	UpdateLODLevel();

	bool bHasSoloEmitter = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (IsSoloParticleInstance(Instance))
		{
			bHasSoloEmitter = true;
			break;
		}
	}

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance != nullptr)
		{
			if (bHasSoloEmitter && !IsSoloParticleInstance(Instance))
			{
				continue;
			}
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
	if (!SpawnEvents.empty() || !DeathEvents.empty() || !CollisionEvents.empty() || !BurstEvents.empty())
	{
		AParticleEventManager* DispatchManager = EventManager;
		if (DispatchManager == nullptr)
		{
			UWorld* World = GetWorld();
			DispatchManager = World != nullptr ? World->GetOrCreateParticleEventManager() : nullptr;

			// Caching ParticleEventManager
			EventManager = DispatchManager;
		}

		if (DispatchManager != nullptr)
		{
			if (!SpawnEvents.empty()) DispatchManager->HandleParticleSpawnEvents(this, SpawnEvents);
			if (!DeathEvents.empty()) DispatchManager->HandleParticleDeathEvents(this, DeathEvents);
			if (!CollisionEvents.empty()) DispatchManager->HandleParticleCollisionEvents(this, CollisionEvents);
			if (!BurstEvents.empty()) DispatchManager->HandleParticleBurstEvents(this, BurstEvents);
		}
	}

	SpawnEvents.clear();
	DeathEvents.clear();
	CollisionEvents.clear();
	BurstEvents.clear();
}

void UParticleSystemComponent::ReportEventSpawn(const FParticleEventSpawnData& Event)
{
	SpawnEvents.push_back(Event);
}

void UParticleSystemComponent::ReportEventDeath(const FParticleEventDeathData& Event)
{
	DeathEvents.push_back(Event);
}

void UParticleSystemComponent::ReportEventCollision(const FParticleEventCollideData& Event)
{
	CollisionEvents.push_back(Event);
}

void UParticleSystemComponent::ReportEventBurst(const FParticleEventBurstData& Event)
{
	BurstEvents.push_back(Event);
}

void UParticleSystemComponent::PackRenderData()
{
	ReleaseRenderData();

	bool bHasSoloEmitter = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (IsSoloParticleInstance(Instance))
		{
			bHasSoloEmitter = true;
			break;
		}
	}

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if (Instance == nullptr || !IsLiveObject(Instance->CurrentLODLevel))
		{
			continue;
		}
		if (!Instance->CurrentLODLevel->bEnabled || (bHasSoloEmitter && !Instance->CurrentLODLevel->bSolo))
		{
			continue;
		}

		UParticleModuleTypeDataBase* TypeDataModule = Instance->CurrentLODLevel->TypeDataModule;
		if (!IsLiveObject(TypeDataModule) || !TypeDataModule->bEnabled)
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

const FDynamicEmitterDataBase* UParticleSystemComponent::GetEmitterRenderDataSnapshot(int32 SnapshotIndex) const
{
	if (SnapshotIndex < 0 || SnapshotIndex >= static_cast<int32>(EmitterRenderData.size()))
	{
		return nullptr;
	}

	return EmitterRenderData[SnapshotIndex];
}

FPrimitiveRenderProxy* UParticleSystemComponent::GetRenderProxy()
{
	return RenderProxy.get();
}

void UParticleSystemComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		const FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if (!Instance || !IsLiveObject(Instance->CurrentLODLevel) || !Instance->CurrentLODLevel->bEnabled)
		{
			continue;
		}

		const UParticleModuleRequired* RequiredModule = Instance->CurrentRuntimeCache != nullptr
			? Instance->CurrentRuntimeCache->RequiredModule
			: nullptr;
		if (RequiredModule == nullptr || !RequiredModule->bUseFixedBounds)
		{
			const FDynamicMeshEmitterData* MeshEmitterData = FindMeshRenderDataForEmitter(EmitterRenderData, EmitterIndex);
			const FStaticMesh* MeshData = MeshEmitterData != nullptr && MeshEmitterData->Mesh != nullptr
				? MeshEmitterData->Mesh->GetMeshData(0)
				: nullptr;
			if (MeshData != nullptr)
			{
				// Conservative visibility/framing bound only; exact picking/raycast remains unsupported for mesh particles.
				const FBoundingBox MeshParticleBounds = ParticleMeshBounds::BuildConservativeWorldBounds(
					MeshEmitterData->GetSource(),
					MeshEmitterData->ComponentToWorld,
					MeshData->LocalBounds);
				if (MeshParticleBounds.IsValid())
				{
					WorldAABB.Merge(MeshParticleBounds);
					continue;
				}
			}
		}

		FVector Min;
		FVector Max;
		Instance->CalculateWorldBounds(Min, Max);
		const FAABB EmitterBounds(Min, Max);

		if (EmitterBounds.IsValid())
		{
			WorldAABB.Merge(EmitterBounds);
		}
	}

	if (!WorldAABB.IsValid())
	{
		const FVector Center = GetWorldLocation();
		const FVector Extent(1.0f, 1.0f, 1.0f);
		WorldAABB.Expand(Center - Extent);
		WorldAABB.Expand(Center + Extent);
	}
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
	ReleaseRenderProxyResources();
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

void UParticleSystemComponent::ReleaseRenderProxyResources()
{
	if (RenderProxy != nullptr)
	{
		RenderProxy->ReleaseResources();
	}
}

int32 UParticleSystemComponent::SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate) const
{
	const UParticleSystem* ParticleSystem = GetTemplate();
	if (ParticleSystem == nullptr || EmitterTemplate == nullptr || EmitterTemplate->LODLevels.size() <= 1)
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

	// ParticleSystem Asset의 LOD가 3이라고 하더라도, 특정 Emitter의 LOD가 1이라면 1을 적용.	
	const int32 ThresholdCount = std::min(
		static_cast<int32>(EmitterTemplate->LODLevels.size()),
		static_cast<int32>(ParticleSystem->LODDistances.size()));
	if (ThresholdCount <= 1)
	{
		return 0;
	}

	const float Distance = FVector::Distance(GetWorldLocation(), ActiveCamera->GetLocation());
	float PreviousThreshold = ParticleSystem->LODDistances[0];
	int32 SelectedIndex = 0;

	// EmitterTemplate과 ParticleSystem의 LODDistance중 더 작은 Lod count를 선택한다.
	// 순회하면서 거리를 체크, Threshold보다 더 멀리있다면 갱신, 아니라면 해당 구간이므로 LOD 인덱스 확정 후 반환.
	for (int32 LODIndex = 1; LODIndex < ThresholdCount; ++LODIndex)
	{
		const float Threshold = ParticleSystem->LODDistances[LODIndex];
		if (Threshold < 0.0f || Threshold < PreviousThreshold)
		{
			// 잘못된 LOD 거리 데이터 이후의 전환은 사용하지 않음.
			break;
		}

		if (Distance < Threshold)
		{
			break;
		}

		SelectedIndex = LODIndex;
		PreviousThreshold = Threshold;
	}

	return SelectedIndex;
}

void UParticleSystemComponent::UpdateLODLevel()
{
	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[static_cast<size_t>(EmitterIndex)];
		if (Instance == nullptr || Instance->SpriteTemplate == nullptr)
		{
			continue;
		}

		// 거리 기반 LOD 선택
		int32 NewLODIndex = SelectLODLevelIndex(Instance->SpriteTemplate);
		if (NewLODIndex != 0 && !Instance->SpriteTemplate->ValidateLODTopology(false))
		{
			// invalid topology fallback
			NewLODIndex = 0;
		}

		if (Instance->CurrentLODLevelIndex == NewLODIndex)
		{
			continue;
		}

		// Cascade-style LOD 전환
		if (!Instance->SetCurrentLODIndex(NewLODIndex))
		{
			UE_LOG_WARNING("[Particle] LOD switch failed. Falling back to LOD 0.");
			Instance->SetCurrentLODIndex(0);
		}

		ReleaseRenderProxyResources();
		ReleaseRenderData();
	}
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
		// 유효한 emitter asset만 instance화
		if (!IsLiveObject(EmitterTemplate))
		{
			continue;
		}

		// 현재 거리 기준 LOD 선택
		const int32 LODIndex = SelectLODLevelIndex(EmitterTemplate);
		if (FParticleEmitterInstance* Instance = CreateEmitterInstanceForLOD(EmitterTemplate, LODIndex))
		{
			EmitterInstances.push_back(Instance);
		}
	}
}

FParticleEmitterInstance* UParticleSystemComponent::CreateEmitterInstanceForLOD(
	UParticleEmitter* EmitterTemplate,
	int32 LODIndex)
{
	// template 방어
	if (!IsLiveObject(EmitterTemplate))
	{
		return nullptr;
	}

	// runtime cache 최신화
	EmitterTemplate->CacheEmitterModuleInfo();

	if (LODIndex != 0 && !EmitterTemplate->ValidateLODTopology(false))
	{
		// invalid topology fallback
		LODIndex = 0;
	}

	// LOD / TypeData 조회
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterTemplate, LODIndex);
	UParticleModuleTypeDataBase* TypeData = GetLiveTypeData(LODLevel);

	if (TypeData == nullptr)
	{
		UE_LOG_WARNING("[Particle] Emitter has no TypeDataModule. Falling back to base particle emitter instance.");
	}

	// TypeData factory 또는 base instance
	FParticleEmitterInstance* Instance = TypeData != nullptr
		? TypeData->CreateInstance(EmitterTemplate, *InstanceOwner)
		: new FParticleEmitterInstance(*InstanceOwner);

	// LOD 기준 초기화
	if (Instance != nullptr && Instance->Init(EmitterTemplate, LODIndex))
	{
		return Instance;
	}

	// init 실패 정리
	delete Instance;
	return nullptr;
}
