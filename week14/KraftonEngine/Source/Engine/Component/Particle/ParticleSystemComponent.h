#pragma once
#include "Component/PrimitiveComponent.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;
class UParticleSystem;

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void SetTemplate(UParticleSystem* InTemplate);

	void ResetSystem();


private:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	UParticleSystem* Template;

	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

};

