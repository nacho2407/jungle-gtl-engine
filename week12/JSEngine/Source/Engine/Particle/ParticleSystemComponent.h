#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"

#include <memory>

class AParticleEventManager;
class UWorld;

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
	void SetEventManager(AParticleEventManager* InEventManager) { EventManager = InEventManager; }
	UWorld* GetWorld() const;

	void TickComponent(float DeltaTime) override;
	void FinalizeTickComponent();
	void PackRenderData();

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return false; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void ResetParticles();

private:
	void ReleaseEmitterInstances();
	void ReleaseRenderData();
	void CreateEmitterInstances();

	AParticleEventManager* EventManager = nullptr;

	class FInstanceOwner;
	std::unique_ptr<FInstanceOwner> InstanceOwner;
};
