#include "ParticleSystemComponent.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <cstring>

#include "Render/Proxy/PrimitiveSceneProxy.h"

UParticleSystemComponent::UParticleSystemComponent()
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UParticleSystemComponent::~UParticleSystemComponent()
{
    ClearRenderData();
    ClearEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
    if (Template.Get() == InTemplate)
    {
        return;
    }

    ClearRenderData();
    ClearEmitterInstances();

    Template     = InTemplate;
    if (Template.Get())
    {
        TemplatePath = Template.Get()->GetSourcePath();
    }
    else
    {
        TemplatePath = "None";
    }

    InitializeSystem();

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void UParticleSystemComponent::InitializeSystem()
{
    if (Template.Get() == nullptr)
    {
        bInitialized = false;
        return;
    }

    ClearEmitterInstances();
    BuildEmitterInstances();

    bInitialized = true;
}

void UParticleSystemComponent::ResetSystem()
{
    ClearRenderData();
    ClearEmitterInstances();

    bInitialized = false;

    if (Template.Get())
    {
        InitializeSystem();
    }

    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
    return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::UpdateWorldAABB() const
{
    const FVector Extent(1.0f, 1.0f, 1.0f);
    const FVector Center = GetWorldLocation();

    WorldAABBMinLocation = Center - Extent;
    WorldAABBMaxLocation = Center + Extent;

    bWorldAABBDirty    = false;
    bHasValidWorldAABB = true;
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Template") == 0 || strcmp(PropertyName, "TemplatePath") == 0)
    {
        if (TemplatePath.IsNull())
        {
            SetTemplate(nullptr);
        }
        else
        {
            UParticleSystem* Loaded = FParticleSystemManager::Get().Load(TemplatePath.ToString());
            SetTemplate(Loaded);
        }
    }
}

void UParticleSystemComponent::PostDuplicate()
{
    UPrimitiveComponent::PostDuplicate();

    if (!TemplatePath.IsNull())
    {
        UParticleSystem* Loaded = FParticleSystemManager::Get().Load(TemplatePath.ToString());
        SetTemplate(Loaded);
    }
}

void UParticleSystemComponent::TickComponent(
    float                        DeltaTime,
    ELevelTick                   TickType,
    FActorComponentTickFunction& ThisTickFunction
    )
{
    UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsActive())
    {
        return;
    }

    if (!Template.Get())
    {
        return;
    }

    if (!bInitialized)
    {
        InitializeSystem();
    }

    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        if (Instance)
        {
            Instance->Tick(DeltaTime, false);
        }
    }

    BuildDynamicData();

    MarkProxyDirty(EDirtyFlag::Mesh);
}

void UParticleSystemComponent::ClearEmitterInstances()
{
    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        delete Instance;
    }

    EmitterInstances.clear();
}

void UParticleSystemComponent::ClearRenderData()
{
    for (FDynamicEmitterDataBase* Data : EmitterRenderData)
    {
        delete Data;
    }

    EmitterRenderData.clear();
}

void UParticleSystemComponent::BuildEmitterInstances()
{
    UParticleSystem* ParticleTemplate = Template.Get();
    if (!ParticleTemplate)
    {
        return;
    }

    for (UParticleEmitter* Emitter : ParticleTemplate->GetEmitters())
    {
        if (!Emitter)
        {
            continue;
        }

        if (!Emitter->HasValidLOD0())
        {
            Emitter->InitializeDefaultSpriteEmitter();
        }

        if (!Emitter->HasValidLOD0())
        {
            return;
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

void UParticleSystemComponent::BuildDynamicData()
{
    ClearRenderData();

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
