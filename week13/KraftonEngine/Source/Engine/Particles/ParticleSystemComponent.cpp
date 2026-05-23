#include "Particles/ParticleSystemComponent.h"
#include "Object/Object.h"

UParticleSystemComponent::~UParticleSystemComponent()
{
    ResetParticles();
}

void UParticleSystemComponent::InitializeSystem()
{
    ResetParticles();

    if (!Template)
    {
        return;
    }

    for (UParticleEmitter* Emitter : Template->GetEmitters())
    {
        if (!Emitter)
        {
            continue;
        }

        Emitter->CacheEmitterModuleInfo();

        FParticleEmitterInstance* Instance = nullptr;
        if (Emitter->bUseMeshInstance)
        {
            Instance = new FParticleMeshEmitterInstance();
        }
        else
        {
            Instance = new FParticleSpriteEmitterInstance();
        }

        Instance->InitParameters(Emitter, this);
        Instance->Init();

        if (UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0))
        {
            Instance->Resize(LODLevel->CalculateMaxActiveParticleCount());
        }
        else
        {
            Instance->Resize(32);
        }

        EmitterInstances.push_back(Instance);
    }
}

void UParticleSystemComponent::ResetParticles()
{
    for (FDynamicEmitterDataBase* Data : EmitterRenderData)
    {
        delete Data;
    }
    EmitterRenderData.clear();

    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        delete Instance;
    }
    EmitterInstances.clear();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        if (Instance)
        {
            Instance->Tick(DeltaTime, false);
        }
    }

    BuildRenderData();
}

void UParticleSystemComponent::BuildRenderData()
{
    for (FDynamicEmitterDataBase* Data : EmitterRenderData)
    {
        delete Data;
    }
    EmitterRenderData.clear();

    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        if (!Instance)
        {
            continue;
        }

        if (FDynamicEmitterDataBase* DynamicData = Instance->GetDynamicData(false))
        {
            EmitterRenderData.push_back(DynamicData);
        }
    }
}
