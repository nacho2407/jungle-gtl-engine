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
#include "Render/Scene/Scene.h"
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

	const FDynamicBeamEmitterData* FindBeamRenderDataForEmitter(
		const TArray<FDynamicEmitterDataBase*>& RenderData,
		int32 EmitterIndex)
	{
		for (const FDynamicEmitterDataBase* EmitterData : RenderData)
		{
			if (EmitterData != nullptr &&
				EmitterData->EmitterIndex == EmitterIndex &&
				EmitterData->GetEmitterType() == EDynamicEmitterType::Beam)
			{
				return static_cast<const FDynamicBeamEmitterData*>(EmitterData);
			}
		}
		return nullptr;
	}

	FVector GetBeamWorldPoint(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FVector& Point)
	{
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformPosition(Point)
			: Point;
	}

	FBoundingBox BuildBeamWorldBounds(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld)
	{
		const FVector Source = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.SourcePoint);
		const FVector Target = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.TargetPoint);
		const float HalfWidth = std::max(ReplayData.BeamWidth, 0.1f) * 0.5f;
		const FVector Extent(HalfWidth, HalfWidth, HalfWidth);

		FBoundingBox Bounds;
		Bounds.Expand(Source - Extent);
		Bounds.Expand(Source + Extent);
		Bounds.Expand(Target - Extent);
		Bounds.Expand(Target + Extent);
		return Bounds;
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

	AActor* GetSourceActor() const override
	{
		return Component != nullptr ? Component->GetOwner() : nullptr;
	}

	bool ParticleLineCheck(
		FHitResult& Hit,
		AActor* SourceActor,
		const FVector& EndWS,
		const FVector& StartWS,
		const FCollisionShape& CollisionShape) override
	{
		return Component != nullptr &&
			Component->ParticleLineCheck(Hit, SourceActor, EndWS, StartWS, CollisionShape);
	}

	void AddCollisionEvent(const FParticleEventCollideData& Event) override
	{
		if (Component != nullptr)
		{
			Component->ReportEventCollision(Event);
		}
	}

	void AddParticleEvent(const FParticleEventPayload& Event) override
	{
		if (Component != nullptr)
		{
			Component->ReportParticleEvent(Event);
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
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
}

void UParticleSystemComponent::SetTemplateAsset(const TSoftObjectPtr<UParticleSystem>& InTemplate)
{
	Template = InTemplate;
	ResolvedTemplate = nullptr;
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
}

void UParticleSystemComponent::SetTemplatePath(const FString& InPath)
{
	Template.SetPath(FPaths::Normalize(InPath));
	ResolvedTemplate = nullptr;
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
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
		ReleaseRenderData();
		ReleaseEmitterInstances();
		CreateEmitterInstances();
		UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
	}
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (PropertyName && FString(PropertyName) == "Template")
	{
		ResolvedTemplate = nullptr;
		ReleaseRenderData();
		ReleaseEmitterInstances();
		CreateEmitterInstances();
		UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
	}
}

UWorld* UParticleSystemComponent::GetWorld() const
{
	return GetOwner() != nullptr ? GetOwner()->GetFocusedWorld() : nullptr;
}

bool UParticleSystemComponent::ParticleLineCheck(
	FHitResult& Hit,
	AActor* SourceActor,
	const FVector& EndWS,
	const FVector& StartWS,
	const FCollisionShape& CollisionShape)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		Hit.Reset();
		return false;
	}

	FCollisionQueryParams Params;
	Params.IgnoredActor = SourceActor;
	Params.IgnoredComponent = this;
	Params.bFindInitialOverlaps = true;

	return CollisionShape.IsNearlyZero()
		? World->LineTraceSingleShapeTarget(Hit, StartWS, EndWS, Params)
		: World->SweepSingleShapeTarget(Hit, StartWS, EndWS, CollisionShape, Params);
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	ParticleEvents.clear();

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
	NotifySpatialIndexDirty();
	
	// Tick이 끝날 때 Event를 처리
	FinalizeTickComponent();
}

void UParticleSystemComponent::FinalizeTickComponent()
{
	if (!CollisionEvents.empty())
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
			DispatchManager->HandleParticleCollisionEvents(this, CollisionEvents);
		}
	}

	CollisionEvents.clear();
}

void UParticleSystemComponent::ReportEventCollision(const FParticleEventCollideData& Event)
{
	CollisionEvents.push_back(Event);
}

void UParticleSystemComponent::ReportParticleEvent(const FParticleEventPayload& Event)
{
	ParticleEvents.push_back(Event);
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
		// solo 상태가 아닌 emitter의 render 차단
		if (bHasSoloEmitter && !Instance->CurrentLODLevel->bSolo)
		{
			continue;
		}

		// LOD 비활성 상태의 기존 live particle render 허용
		// bEnabled=false LOD는 emitter tick에서 spawn/update만 멈추고 age/kill은 계속 진행되므로,
		// render snapshot은 계속 만들어야 기존 particle이 수명에 따라 자연스럽게 사라짐
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

void UParticleSystemComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		const FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if (!Instance || !IsLiveObject(Instance->CurrentLODLevel))
		{
			continue;
		}

		// LOD 비활성 상태의 기존 live particle bounds 유지
		// render는 허용되므로 spatial bounds도 함께 유지하여 자연 소멸 중인 particle이 잘리지 않도록 함
		const UParticleModuleRequired* RequiredModule = Instance->CurrentRuntimeCache != nullptr
			? Instance->CurrentRuntimeCache->RequiredModule
			: nullptr;
		if (RequiredModule == nullptr || !RequiredModule->bUseFixedBounds)
		{
			const FDynamicBeamEmitterData* BeamEmitterData = FindBeamRenderDataForEmitter(EmitterRenderData, EmitterIndex);
			if (BeamEmitterData != nullptr)
			{
				const FBoundingBox BeamBounds = BuildBeamWorldBounds(
					BeamEmitterData->ReplayData,
					BeamEmitterData->ComponentToWorld);
				if (BeamBounds.IsValid())
				{
					WorldAABB.Merge(BeamBounds);
					continue;
				}
			}

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
	ReleaseRenderData();
	ReleaseEmitterInstances();
	CreateEmitterInstances();
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
}

void UParticleSystemComponent::ReleaseEmitterInstances()
{
	ParticleEvents.clear();

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

int32 UParticleSystemComponent::SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate, int32 CurrentLODIndex) const
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

	// 순수 거리 기준 LOD 후보 계산
	// EmitterTemplate과 ParticleSystem의 LODDistance 중 더 작은 LOD count를 사용
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

	// hysteresis 비활성 또는 LOD 일시적 유지가 불필요한 상태 처리
	const int32 ClampedCurrentLODIndex = std::clamp(CurrentLODIndex, 0, ThresholdCount - 1);
	const float HysteresisDistance = std::max(ParticleSystem->LODHysteresisDistance, 0.0f);
	if (HysteresisDistance <= 0.0f || SelectedIndex == ClampedCurrentLODIndex)
	{
		return SelectedIndex;
	}

	// 더 먼 LOD로 내려가는 경우
	// 현재 LOD의 다음 threshold를 hysteresis만큼 지나야 전환 허용
	if (SelectedIndex > ClampedCurrentLODIndex)
	{
		const int32 NextLODIndex = ClampedCurrentLODIndex + 1;
		if (NextLODIndex < ThresholdCount)
		{
			const float SwitchOutDistance = ParticleSystem->LODDistances[NextLODIndex] + HysteresisDistance;
			if (Distance < SwitchOutDistance)
			{
				return ClampedCurrentLODIndex;
			}
		}
	}

	// 더 가까운 LOD로 올라가는 경우
	// 현재 LOD의 시작 threshold보다 hysteresis만큼 안쪽으로 들어와야 전환 허용
	if (SelectedIndex < ClampedCurrentLODIndex)
	{
		const float CurrentLODStartDistance = ParticleSystem->LODDistances[ClampedCurrentLODIndex];
		const float SwitchInDistance = std::max(0.0f, CurrentLODStartDistance - HysteresisDistance);
		if (Distance >= SwitchInDistance)
		{
			return ClampedCurrentLODIndex;
		}
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
		int32 NewLODIndex = SelectLODLevelIndex(Instance->SpriteTemplate, Instance->CurrentLODLevelIndex);
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

		ReleaseRenderData();
		UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::ParticleTemplate);
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
		const int32 LODIndex = SelectLODLevelIndex(EmitterTemplate, 0);
		if (FParticleEmitterInstance* Instance = CreateEmitterInstanceForLOD(EmitterTemplate, LODIndex))
		{
			Instance->SetEmitterIndex(static_cast<int32>(EmitterInstances.size()));
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
