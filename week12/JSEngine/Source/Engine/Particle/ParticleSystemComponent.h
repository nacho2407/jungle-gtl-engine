#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleAsset.h"

class UWorld;
class UParticleSystem;
class AParticleEventManager;
class FParticleEmitterInstance;
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

	void Serialize(FArchive& Ar) override;
    void PostEditProperty(const char* PropertyName) override;

	void SetTemplate(UParticleSystem* InTemplate);
	void SetEventManager(AParticleEventManager* InEventManager) { EventManager = InEventManager; }
	UWorld* GetWorld() const;

	void TickComponent(float DeltaTime) override;
	void FinalizeTickComponent();
	void PackRenderData();

	// --- Particle Event Section ---
	void ReportEventSpawn(const FParticleEventSpawnData& Event);
	void ReportEventDeath(const FParticleEventDeathData& Event);
	void ReportEventCollision(const FParticleEventCollideData& Event);
	void ReportEventBurst(const FParticleEventBurstData& Event);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
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
	int32 SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate) const;
	void UpdateLODLevel();
	// Particle Asset Template Reference 설정
    void ResolveTemplateAssetReference();

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	// Asset / Runtime
	UParticleSystem* Template = nullptr;

	UPROPERTY(DisplayName = "Particle System")
    TSoftObjectPtr<UParticleSystem> TemplateAssetPath;

	// TODO: ParticleSystem Asset의 LOD 거리 계약이 정해지면 컴포넌트 균등 간격 대신 해당 설정을 사용한다.
	UPROPERTY(DisplayName = "LOD Distance Interval", Min = 0.0f, Speed = 10.0f)
	float LODDistanceInterval = 1000.0f;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// Event Queue
	TArray<FParticleEventSpawnData> SpawnEvents;
	TArray<FParticleEventDeathData> DeathEvents;
	TArray<FParticleEventCollideData> CollisionEvents;
	TArray<FParticleEventBurstData> BurstEvents;

	AParticleEventManager* EventManager = nullptr;

	// CPP참고 -  EmitterInstance에게 넘겨주는 Component 정보
	class FInstanceOwner;
	std::unique_ptr<FInstanceOwner> InstanceOwner;
};
