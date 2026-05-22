#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
class UParticleSystem;

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

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void LoadTemplateFromPath();

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TObjectPtr<UParticleSystem> ParticleSystem;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Particle System", AssetType="UParticleSystem")
	FSoftObjectPtr ParticleSystemPath = "None";
};
