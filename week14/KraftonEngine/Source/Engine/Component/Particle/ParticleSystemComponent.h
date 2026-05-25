#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Particles/Runtime/ParticleCollisionEvent.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
class UParticleEmitter;
class UParticleSystem;
class UParticleSystemComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FParticleCollideEventSignature,
	UParticleSystemComponent* /*ParticleSystemComponent*/,
	const FParticleCollisionEventPayload& /*CollisionEvent*/);

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return ParticleSystem.Get(); }

	void ResetSystem();
	void Activate() override;
	void Deactivate() override;
	void EndPlay() override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void PostDuplicate() override;
	void PostEditProperty(const char* PropertyName) override;

	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }
	int32 GetCurrentLODIndex() const { return CurrentLODIndex; }
	void SetPreviewSoloEmitterIndex(int32 InEmitterIndex);
	int32 GetPreviewSoloEmitterIndex() const { return PreviewSoloEmitterIndex; }
	void BroadcastParticleCollisionEvent(const FParticleCollisionEventPayload& Event);

	FParticleCollideEventSignature OnParticleCollideEvent;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	bool ShouldCreateEmitterInstance(int32 EmitterIndex, const UParticleEmitter* Emitter) const;
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void LoadTemplateFromPath();

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TObjectPtr<UParticleSystem> ParticleSystem;
	int32 CurrentLODIndex = 0;
	int32 PreviewSoloEmitterIndex = -1;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Particle System", AssetType="UParticleSystem")
	FSoftObjectPtr ParticleSystemPath = "None";
};
