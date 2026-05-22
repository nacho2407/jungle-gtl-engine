#pragma once

#include "Component/PrimitiveComponent.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

struct FDynamicEmitterDataBase;
struct FParticleEmitterInstance;
class UParticleSystem;

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return ParticleSystemTemplate; }

	void ResetSystem();
	void Activate() override;
	void Deactivate() override;
	void EndPlay() override;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void ClearEmitterRenderData();

	TArray<FParticleEmitterInstance*> EmitterInstances;
	UParticleSystem* ParticleSystemTemplate = nullptr;

	TArray<FDynamicEmitterDataBase*> EmitterRenderData;
};
