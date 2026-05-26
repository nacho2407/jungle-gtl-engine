#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleAsset.h"

class UWorld;
class UParticleSystem;
class AParticleEventManager;
class FParticleEmitterInstance;
class FParticleSystemRenderProxy;
class FPrimitiveRenderProxy;
class UParticleSystemComponent;

struct FParticleEventSpawnData;
struct FParticleEventDeathData;
struct FParticleEventBurstData;
struct FDynamicEmitterDataBase;
struct FParticleEventCollideData;

// --- Particle Event Delegate ---
// TODO: Core 이벤트 데이터에 이름, 시간, 속도 정보가 추가되면 시그니처 확장
DECLARE_DELEGATE(FParticleSpawnSignature, UParticleSystemComponent*, const FParticleEventSpawnData&)
DECLARE_DELEGATE(FParticleDeathSignature, UParticleSystemComponent*, const FParticleEventDeathData&)
DECLARE_DELEGATE(FParticleCollisionSignature, UParticleSystemComponent*, const FParticleEventCollideData&)
DECLARE_DELEGATE(FParticleBurstSignature, UParticleSystemComponent*, const FParticleEventBurstData&)

/******************************************************************
* Particle System 런타임 객체 관리 컴포넌트
* Particle System이 Asset이라면 PSC는 Asset을 참조하는 개별 Instance
*******************************************************************/
UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	void SetTemplateAsset(const TSoftObjectPtr<UParticleSystem>& InTemplate);
	void SetTemplatePath(const FString& InPath);

	UParticleSystem* GetTemplate();
	const UParticleSystem* GetTemplate() const;

	void SetEventManager(AParticleEventManager* InEventManager) { EventManager = InEventManager; }
	UWorld* GetWorld() const;

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	void TickComponent(float DeltaTime) override;
	void FinalizeTickComponent();
	void PackRenderData();
	int32 GetEmitterRenderDataSnapshotCount() const { return static_cast<int32>(EmitterRenderData.size()); }
	const FDynamicEmitterDataBase* GetEmitterRenderDataSnapshot(int32 SnapshotIndex) const;

	// --- Particle Event Section ---
	void ReportEventSpawn(const FParticleEventSpawnData& Event);
	void ReportEventDeath(const FParticleEventDeathData& Event);
	void ReportEventCollision(const FParticleEventCollideData& Event);
	void ReportEventBurst(const FParticleEventBurstData& Event);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	FPrimitiveRenderProxy* GetRenderProxy() override;
	bool SupportsOutline() const override { return false; }

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void ResetParticles();

	FParticleSpawnSignature OnParticleSpawn;
	FParticleDeathSignature OnParticleDeath;
	FParticleCollisionSignature OnParticleCollide;
	FParticleBurstSignature OnParticleBurst;

private:
	void CreateEmitterInstances();
	void ReleaseEmitterInstances();
	void ReleaseRenderData();
	void ReleaseRenderProxyResources();
	int32 SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate) const;
	void UpdateLODLevel();

	/**
	 * @brief 지정된 emitter template과 LOD index에 맞는 새 emitter instance를 생성합니다.
	 *
	 * @param EmitterTemplate instance를 만들 particle emitter template
	 *
	 * @param LODIndex 생성할 instance가 사용할 LOD index
	 *
	 * @return 생성과 초기화에 성공한 emitter instance. 실패하면 nullptr 반환
	 *
	 * @details TypeDataModule이 있으면 TypeDataModule의 factory를 사용하고, 없으면 기본 FParticleEmitterInstance로 fallback합니다.
	 */
	FParticleEmitterInstance* CreateEmitterInstanceForLOD(UParticleEmitter* EmitterTemplate, int32 LODIndex);

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// Event Queue
	TArray<FParticleEventSpawnData> SpawnEvents;
	TArray<FParticleEventDeathData> DeathEvents;
	TArray<FParticleEventCollideData> CollisionEvents;
	TArray<FParticleEventBurstData> BurstEvents;

	AParticleEventManager* EventManager = nullptr;
	UParticleSystem* ResolvedTemplate = nullptr;

	// CPP참고 -  EmitterInstance에게 넘겨주는 Component 정보
	class FInstanceOwner;
	std::unique_ptr<FInstanceOwner> InstanceOwner;
	std::unique_ptr<FParticleSystemRenderProxy> RenderProxy;

	UPROPERTY(DisplayName = "Template", ReferenceType = Asset)
	TSoftObjectPtr<UParticleSystem> Template;
};
