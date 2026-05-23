#include "ParticleSystemComponent.h"

#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"
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
        // Instance->Tick(DeltaTime);
    }

    BuildDynamicData();

    MarkProxyDirty(EDirtyFlag::Mesh);
}

void UParticleSystemComponent::ClearEmitterInstances()
{
    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        // TODO: 나중에 Delete 할지말지 고민 
        (void)Instance;
    }

    EmitterInstances.clear();
}

void UParticleSystemComponent::ClearRenderData()
{
    for (FDynamicEmitterDataBase* Data : EmitterRenderData)
    {
        // TODO: 나중에 Delete 할지말지 고민 
        (void)Data;
    }

    EmitterRenderData.clear();
}

void UParticleSystemComponent::BuildEmitterInstances()
{}

void UParticleSystemComponent::BuildDynamicData()
{}
