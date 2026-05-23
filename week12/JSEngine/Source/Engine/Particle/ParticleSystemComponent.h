#pragma once

#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Particle/ParticleAsset.h"
#include <memory>

class UWorld;


/**
 * IParticleEmitterInstanceOwner
 * - EmitterInstance가 자신을 보유한 Component 정보에 접근하기 위한 Interface
 * - 필요한 API가 있다면 InstanceOwner에 추가하고 cpp에 구현
 */
class IParticleEmitterInstanceOwner
{
public:
	virtual ~IParticleEmitterInstanceOwner() = default;

    virtual UWorld* GetWorld() const = 0;
    virtual FVector GetWorldLocation() const = 0;
    virtual FMatrix GetComponentToWorld() const = 0;

    virtual void AddSpawnEvent(const FParticleEventSpawnData& Event) = 0;
    virtual void AddDeathEvent(const FParticleEventDeathData& Event) = 0;
    virtual void AddCollisionEvent(const FParticleEventCollideData& Event) = 0;
    virtual void AddBurstEvent(const FParticleEventBurstData& Event) = 0;
};

class FParticleEmitterInstance
{
public:
    FParticleEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
        : Owner(InOwner) {}

	virtual ~FParticleEmitterInstance() = default;

	IParticleEmitterInstanceOwner& GetOwner() { return Owner; }
    const IParticleEmitterInstanceOwner& GetOwner() const { return Owner; }

	UParticleEmitter* SpriteTemplate = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;
	int32 ParticleStride = 0;
	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;

	virtual void Tick(float DeltaTime);

private:
    IParticleEmitterInstanceOwner& Owner;
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
    explicit FParticleMeshEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
        : FParticleEmitterInstance(InOwner)
    {
    }
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
    explicit FParticleBeamEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
        : FParticleEmitterInstance(InOwner)
    {
    }
};

class FParticleTrailsEmitterInstance : public FParticleEmitterInstance
{
public:
    explicit FParticleTrailsEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
        : FParticleEmitterInstance(InOwner)
    {
    }
};

class FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance
{
public:
    explicit FParticleRibbonEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
        : FParticleTrailsEmitterInstance(InOwner)
    {
    }
};

UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	UParticleSystem* Template = nullptr;
	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	TArray<FParticleEventSpawnData> SpawnEvents;
	TArray<FParticleEventDeathData> DeathEvents;
	TArray<FParticleEventCollideData> CollisionEvents;
	TArray<FParticleEventBurstData> BurstEvents;

	void SetTemplate(UParticleSystem* InTemplate);
	void SetEventManager(class AParticleEventManager* InEventManager) { EventManager = InEventManager; }
	UWorld* GetWorld() const;

	void TickComponent(float DeltaTime) override;
	void FinalizeTickComponent();
	void PackRenderData();

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return false; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

private:
	void ReleaseEmitterInstances();
	void ReleaseRenderData();
	void CreateEmitterInstances();

	AParticleEventManager* EventManager = nullptr;

    class FInstanceOwner;
    std::unique_ptr<FInstanceOwner> InstanceOwner;
};

UCLASS()
class AParticleEventManager : public AActor
{
public:
	GENERATED_BODY(AParticleEventManager, AActor)

	void HandleParticleSpawnEvents(UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& InSpawnEvents);
	void HandleParticleDeathEvents(UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& InDeathEvents);
	void HandleParticleCollisionEvents(UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& InCollisionEvents);
	void HandleParticleBurstEvents(UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& InBurstEvents);
};
