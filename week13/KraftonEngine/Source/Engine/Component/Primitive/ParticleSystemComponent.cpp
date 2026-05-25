#include "ParticleSystemComponent.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <cstring>

#include "Object/GarbageCollection.h"
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
	CachedWorldTimeSeconds = 0.0f;

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
    FBoundingBox CombinedBounds;

    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        if (!Instance)
        {
            continue;
        }

        FBoundingBox EmitterBounds = Instance->GetBoundingBox();
        if (!EmitterBounds.IsValid())
        {
            continue;
        }

        CombinedBounds.Expand(EmitterBounds.Min);
        CombinedBounds.Expand(EmitterBounds.Max);
    }

    if (CombinedBounds.IsValid())
    {
        WorldAABBMinLocation = CombinedBounds.Min;
        WorldAABBMaxLocation = CombinedBounds.Max;
    }
    else
    {
        // 아직 파티클이 생성되기 전 fallback
        const FVector Center = GetWorldLocation();

        const FVector FallbackCenter = Center;
        const FVector FallbackExtent = FVector(100.0f, 100.0f, 100.0f);

        WorldAABBMinLocation = FallbackCenter - FallbackExtent;
        WorldAABBMaxLocation = FallbackCenter + FallbackExtent;
    }

    bWorldAABBDirty = false;
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

	CachedWorldTimeSeconds += DeltaTime;

	int32 CalculatedLODIndex = 0;
	if (Template.Get() && !Template->LODDistances.empty())
	{
		for (int32 i = 0; i < static_cast<int32>(Template->LODDistances.size()); i++)
		{
			if (CachedDistanceToCamera >= Template->LODDistances[i])
				CalculatedLODIndex = i;
			else
				break;
		}
	}

	int32 TargetLODIndex = CalculatedLODIndex;
    for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
    {
        FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
        if (Instance)
        {
			if (Instance->SpriteTemplate)
			{
				int32 MaxLODCount = static_cast<int32>(Instance->SpriteTemplate->GetLODLevels().size());
				int32 ResolvedLOD = std::clamp(TargetLODIndex, 0, MaxLODCount - 1);
				Instance->SetCurrentLODIndex(ResolvedLOD, false);
			}

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

void UParticleSystemComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
    UPrimitiveComponent::AddReferencedObjects(Collector);

    Collector.AddReferencedObject(Template.Get(), "UParticleSystemComponent.Template");

    for (UMaterial* Material : EmitterMaterials)
    {
        Collector.AddReferencedObject(Material, "UParticleSystemComponent.EmitterMaterials");
    }

    for (FParticleEmitterInstance* Instance : EmitterInstances)
    {
        if (Instance)
        {
            Instance->AddReferencedObjects(Collector);
        }
    }

    for (FDynamicEmitterDataBase* Data : EmitterRenderData)
    {
        if (!Data)
        {
            continue;
        }

        const FDynamicEmitterReplayDataBase& Source = Data->GetSource();
        if (Source.eEmitterType == EDynamicEmitterType::Sprite ||
            Source.eEmitterType == EDynamicEmitterType::Mesh ||
            Source.eEmitterType == EDynamicEmitterType::Beam ||
            Source.eEmitterType == EDynamicEmitterType::Ribbon)
        {
            const FDynamicSpriteEmitterReplayDataBase& SpriteSource =
                static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
            Collector.AddReferencedObject(SpriteSource.Material, "UParticleSystemComponent.DynamicData.Material");
            Collector.AddReferencedObject(SpriteSource.RequiredModule, "UParticleSystemComponent.DynamicData.RequiredModule");
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

        // Disabled emitter must not spawn or render. Without this skip,
        // toggling bEnabled in the asset editor has no visible effect.
        if (!Emitter->IsEnabled())
        {
            continue;
        }

        if (!Emitter->HasValidLOD0())
        {
            Emitter->InitializeDefaultSpriteEmitter();
        }

        if (!Emitter->HasValidLOD0())
        {
            continue;
        }

        Emitter->CacheEmitterModuleInfo();

        FParticleEmitterInstance* Instance = nullptr;
        if (UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0))
        {
            if (LODLevel->TypeDataModule)
            {
                Instance = LODLevel->TypeDataModule->CreateInstance(Emitter, *this);
            }
        }

        if (!Instance && Emitter->bUseMeshInstance)
        {
            Instance = new FParticleMeshEmitterInstance();
        }
        else if (!Instance)
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
