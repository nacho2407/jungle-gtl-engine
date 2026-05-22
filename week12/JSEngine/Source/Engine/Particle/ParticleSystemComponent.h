#pragma once

#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Particle/ParticleAsset.h"

class UWorld;

class IParticleEmitterInstanceOwner
{
public:
	virtual ~IParticleEmitterInstanceOwner() = default;
	virtual class UParticleSystemComponent* GetParticleSystemComponent() = 0;
};

class FParticleEmitterInstance
{
public:
	virtual ~FParticleEmitterInstance() = default;

	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;
	int32 ParticleStride = 0;
	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;

	virtual void Tick(float DeltaTime);
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
};

class FParticleTrailsEmitterInstance : public FParticleEmitterInstance
{
};

class FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance
{
};

UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent, public IParticleEmitterInstanceOwner
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	UParticleSystemComponent* GetParticleSystemComponent() override { return this; }

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
