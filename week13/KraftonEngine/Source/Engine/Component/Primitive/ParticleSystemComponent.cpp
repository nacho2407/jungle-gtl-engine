#include "ParticleSystemComponent.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

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

    ResolveEmitterMaterialsFromSlots();
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

void UParticleSystemComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
    if (ElementIndex < 0)
    {
        return;
    }

    const int32 RequiredSize = ElementIndex + 1;
    if (static_cast<int32>(EmitterMaterials.size()) < RequiredSize)
    {
        EmitterMaterials.resize(RequiredSize, nullptr);
    }
    if (static_cast<int32>(EmitterMaterialSlots.size()) < RequiredSize)
    {
        EmitterMaterialSlots.resize(RequiredSize, FSoftObjectPtr(FString("None")));
    }

    EmitterMaterials[ElementIndex] = InMaterial;
    EmitterMaterialSlots[ElementIndex] = InMaterial
        ? InMaterial->GetAssetPathFileName()
        : "None";

    for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
    {
        if (FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex])
        {
            Instance->Tick_MaterialOverrides(EmitterIndex);
        }
    }

    BuildDynamicData();
    MarkProxyDirty(EDirtyFlag::Material);
    MarkProxyDirty(EDirtyFlag::Mesh);
}

UMaterial* UParticleSystemComponent::GetMaterial(int32 ElementIndex) const
{
    if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(EmitterMaterials.size()) &&
        EmitterMaterials[ElementIndex])
    {
        return EmitterMaterials[ElementIndex];
    }

    UParticleSystem* ParticleTemplate = Template.Get();
    if (!ParticleTemplate ||
        ElementIndex < 0 ||
        ElementIndex >= static_cast<int32>(ParticleTemplate->GetEmitters().size()))
    {
        return nullptr;
    }

    UParticleEmitter* Emitter = ParticleTemplate->GetEmitters()[ElementIndex];
    UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(0) : nullptr;
    return (LODLevel && LODLevel->RequiredModule) ? LODLevel->RequiredModule->Material : nullptr;
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
    else if (strcmp(PropertyName, "EmitterMaterialSlots") == 0 ||
             strcmp(PropertyName, "EmitterMaterials") == 0)
    {
        ResolveEmitterMaterialsFromSlots();

        for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
        {
            if (FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex])
            {
                Instance->Tick_MaterialOverrides(EmitterIndex);
            }
        }

        BuildDynamicData();
        MarkProxyDirty(EDirtyFlag::Material);
        MarkProxyDirty(EDirtyFlag::Mesh);
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
    else
    {
        ResolveEmitterMaterialsFromSlots();
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

    for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
    {
        FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
        if (Instance)
        {
            Instance->Tick(DeltaTime, false);
            Instance->Tick_MaterialOverrides(EmitterIndex);
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

void UParticleSystemComponent::ResolveEmitterMaterialsFromSlots()
{
    int32 EmitterCount = 0;
    if (UParticleSystem* ParticleTemplate = Template.Get())
    {
        EmitterCount = static_cast<int32>(ParticleTemplate->GetEmitters().size());
    }

    if (EmitterCount > 0)
    {
        if (static_cast<int32>(EmitterMaterialSlots.size()) < EmitterCount)
        {
            EmitterMaterialSlots.resize(EmitterCount, FSoftObjectPtr(FString("None")));
        }
        if (static_cast<int32>(EmitterMaterials.size()) < EmitterCount)
        {
            EmitterMaterials.resize(EmitterCount, nullptr);
        }
    }

    for (int32 Index = 0; Index < static_cast<int32>(EmitterMaterialSlots.size()); ++Index)
    {
        const FSoftObjectPtr& Slot = EmitterMaterialSlots[Index];
        if (Slot.IsNull() || Slot == "None" || Slot.empty())
        {
            EmitterMaterialSlots[Index] = "None";
            if (Index < static_cast<int32>(EmitterMaterials.size()))
            {
                EmitterMaterials[Index] = nullptr;
            }
            continue;
        }

        if (Index >= static_cast<int32>(EmitterMaterials.size()))
        {
            EmitterMaterials.resize(Index + 1, nullptr);
        }
        EmitterMaterials[Index] = FMaterialManager::Get().GetOrCreateMaterial(Slot.ToString());
        if (!EmitterMaterials[Index])
        {
            EmitterMaterialSlots[Index] = "None";
        }
    }
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
