#pragma once

#include "Component/PrimitiveComponent.h"

class UWorld;
class UParticleSystem;
class AParticleEventManager;
class FParticleEmitterInstance;

struct FParticleEventSpawnData;
struct FParticleEventDeathData;
struct FParticleEventBurstData;
struct FDynamicEmitterDataBase;
struct FParticleEventCollideData;

/******************************************************
* Particle System 런타임 객체 관리 컴포넌트
* Particle System이 Asset이라면 PSC는 UParticleSystem을 복제해서 Runtime에
******************************************************/
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

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return false; }

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void ResetParticles();

private:
	void CreateEmitterInstances();
	void ReleaseEmitterInstances();
	void ReleaseRenderData();
	// Particle Asset Template Reference 설정
    void ResolveTemplateAssetReference();

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	// Asset / Runtime
	UParticleSystem* Template = nullptr;

	UPROPERTY(DisplayName = "Particle System")
    TSoftObjectPtr<UParticleSystem> TemplateAssetPath;

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
