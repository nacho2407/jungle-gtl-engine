#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/FName.h"
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

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FParticleEventSignature,
	UParticleSystemComponent* /*ParticleSystemComponent*/,
	const FParticleCollisionEventPayload& /*ParticleEvent*/);

USTRUCT()
struct FParticleSysParam
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Particle")
	FName Name = FName::None;

	UPROPERTY(Edit, Save, Category="Particle")
	FVector Vector = FVector::ZeroVector;
};

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return ParticleSystem.Get(); }

	void SetEmitterSpawningEnabled(bool bEnabled);
	void SetVectorParameter(const FName& ParameterName, const FVector& Value);
	void SetVectorParameter(const FString& ParameterName, const FVector& Value);
	bool GetVectorParameter(const FName& ParameterName, FVector& OutValue) const;
	bool GetVectorParameter(const FString& ParameterName, FVector& OutValue) const;

	void ResetSystem();
	void Activate() override;
	void Deactivate() override;
	void EndPlay() override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void PostDuplicate() override;
	void PostEditProperty(const char* PropertyName) override;

	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }
	int32 GetCurrentLODIndex() const { return CurrentLODIndex; }
	void SetPreviewLODIndex(int32 InLODIndex);
	int32 GetPreviewLODIndex() const { return PreviewLODIndex; }
	void SetPreviewSoloEmitterIndex(int32 InEmitterIndex);
	int32 GetPreviewSoloEmitterIndex() const { return PreviewSoloEmitterIndex; }
	void BroadcastParticleEvent(const FParticleCollisionEventPayload& Event);
	void BroadcastParticleCollisionEvent(const FParticleCollisionEventPayload& Event);

	FParticleEventSignature OnParticleEvent;
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

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Instance Parameters")
	TArray<FParticleSysParam> InstanceParameters;

	int32 CurrentLODIndex = 0;
	int32 PreviewLODIndex = -1;
	int32 PreviewSoloEmitterIndex = -1;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Particle System", AssetType="UParticleSystem")
	FSoftObjectPtr ParticleSystemPath = "None";
};
