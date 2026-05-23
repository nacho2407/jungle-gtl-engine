#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particle/ParticleSystem.h"

#include "Source/Engine/Particles/ParticleSystemComponent.generated.h"

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Particle", DisplayName="Template")
    UParticleSystem* Template = nullptr;

    TArray<FParticleEmitterInstance*> EmitterInstances;
    TArray<FDynamicEmitterDataBase*> EmitterRenderData;

    ~UParticleSystemComponent() override;

    void InitializeSystem();
    void ResetParticles();
    void TickComponent(float DeltaTime);
    void BuildRenderData();

    bool IsGameWorld() const { return true; }
};
