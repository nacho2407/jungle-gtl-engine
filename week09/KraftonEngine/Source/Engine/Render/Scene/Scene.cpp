// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Scene.h"

#include <algorithm>

#include "Component/LightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Profiling/Stats.h"

namespace
{
template <typename ProxyT>
void EnqueueDirtyProxy(TArray<ProxyT*>& DirtyList, ProxyT* Proxy)
{
    if (!Proxy || Proxy->bQueuedForDirtyUpdate)
    {
        return;
    }

    Proxy->bQueuedForDirtyUpdate = true;
    DirtyList.push_back(Proxy);
}

void RemoveSelectedProxyFast(TArray<FPrimitiveSceneProxy*>& SelectedList, FPrimitiveSceneProxy* Proxy)
{
    if (!Proxy || Proxy->SelectedListIndex == UINT32_MAX)
    {
        return;
    }

    const uint32 Index     = Proxy->SelectedListIndex;
    const uint32 LastIndex = static_cast<uint32>(SelectedList.size() - 1);
    if (Index != LastIndex)
    {
        FPrimitiveSceneProxy* LastProxy = SelectedList.back();
        SelectedList[Index]             = LastProxy;
        LastProxy->SelectedListIndex    = Index;
    }

    SelectedList.pop_back();
    Proxy->SelectedListIndex = UINT32_MAX;
}
} // namespace

FScene::~FScene()
{
    for (FPrimitiveSceneProxy* Proxy : PrimitiveProxyRegistry.Proxies)
    {
        delete Proxy;
    }
    PrimitiveProxyRegistry.Reset();

    for (FLightSceneProxy* Proxy : LightProxyRegistry.Proxies)
    {
        delete Proxy;
    }
    LightProxyRegistry.Reset();

    for (FFogSceneProxy* Proxy : FogProxyRegistry.Proxies)
    {
        delete Proxy;
    }
    FogProxyRegistry.Reset();
}

void FScene::RegisterPrimitiveProxy(FPrimitiveSceneProxy* Proxy)
{
    if (!Proxy)
    {
        return;
    }

    Proxy->DirtyFlags = ESceneProxyDirtyFlag::All;

    if (!PrimitiveProxyRegistry.FreeSlots.empty())
    {
        const uint32 Slot = PrimitiveProxyRegistry.FreeSlots.back();
        PrimitiveProxyRegistry.FreeSlots.pop_back();
        Proxy->ProxyId                       = Slot;
        PrimitiveProxyRegistry.Proxies[Slot] = Proxy;
    }
    else
    {
        Proxy->ProxyId = static_cast<uint32>(PrimitiveProxyRegistry.Proxies.size());
        PrimitiveProxyRegistry.Proxies.push_back(Proxy);
    }

    EnqueueDirtyProxy(PrimitiveProxyRegistry.DirtyProxies, Proxy);

    if (Proxy->bNeverCull)
    {
        PrimitiveProxyRegistry.NeverCullProxies.push_back(Proxy);
    }
}

FPrimitiveSceneProxy* FScene::AddPrimitive(UPrimitiveComponent* Component)
{
    if (!Component)
    {
        return nullptr;
    }

    FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
    if (!Proxy)
    {
        return nullptr;
    }

    RegisterPrimitiveProxy(Proxy);
    return Proxy;
}

void FScene::RemovePrimitive(FPrimitiveSceneProxy* Proxy)
{
    if (!Proxy || Proxy->ProxyId == UINT32_MAX)
    {
        return;
    }

    const uint32 Slot = Proxy->ProxyId;

    if (Proxy->bQueuedForDirtyUpdate)
    {
        auto DirtyIt = std::find(PrimitiveProxyRegistry.DirtyProxies.begin(), PrimitiveProxyRegistry.DirtyProxies.end(), Proxy);
        if (DirtyIt != PrimitiveProxyRegistry.DirtyProxies.end())
        {
            *DirtyIt = PrimitiveProxyRegistry.DirtyProxies.back();
            PrimitiveProxyRegistry.DirtyProxies.pop_back();
        }
        Proxy->bQueuedForDirtyUpdate = false;
    }

    if (Proxy->SelectedListIndex != UINT32_MAX)
    {
        RemoveSelectedProxyFast(PrimitiveProxyRegistry.SelectedProxies, Proxy);
    }

    if (Proxy->bNeverCull)
    {
        auto It = std::find(PrimitiveProxyRegistry.NeverCullProxies.begin(), PrimitiveProxyRegistry.NeverCullProxies.end(), Proxy);
        if (It != PrimitiveProxyRegistry.NeverCullProxies.end())
        {
            PrimitiveProxyRegistry.NeverCullProxies.erase(It);
        }
    }

    PrimitiveProxyRegistry.Proxies[Slot] = nullptr;
    PrimitiveProxyRegistry.FreeSlots.push_back(Slot);

    delete Proxy;
}

void FScene::UpdateDirtyProxies()
{
    SCOPE_STAT_CAT("UpdateDirtyProxies", "3_Collect");

    TArray<FPrimitiveSceneProxy*> PendingDirtyProxies = std::move(PrimitiveProxyRegistry.DirtyProxies);
    PrimitiveProxyRegistry.DirtyProxies.clear();

    for (FPrimitiveSceneProxy* Proxy : PendingDirtyProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        Proxy->bQueuedForDirtyUpdate = false;
        if (!Proxy->Owner)
        {
            continue;
        }

        const ESceneProxyDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
        Proxy->DirtyFlags                         = ESceneProxyDirtyFlag::None;

        if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Mesh))
        {
            Proxy->UpdateMesh();
        }
        else if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Material))
        {
            Proxy->UpdateMaterial();
        }

        if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Transform))
        {
            Proxy->UpdateTransform();
        }
        if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Visibility))
        {
            Proxy->UpdateVisibility();
        }
    }
}

void FScene::UpdateDirtyLightProxies()
{
    TArray<FLightSceneProxy*> Pending = std::move(LightProxyRegistry.DirtyProxies);
    LightProxyRegistry.DirtyProxies.clear();

    for (FLightSceneProxy* Proxy : Pending)
    {
        if (!Proxy)
        {
            continue;
        }

        Proxy->bQueuedForDirtyUpdate = false;
        if (!Proxy->Owner)
        {
            continue;
        }

        const ESceneProxyDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
        Proxy->DirtyFlags                         = ESceneProxyDirtyFlag::None;

        if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Transform))
        {
            Proxy->UpdateTransform();
        }

        if (HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Material) ||
            HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Visibility) ||
            HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Lighting) ||
            HasFlag(FlagsToProcess, ESceneProxyDirtyFlag::Shadow))
        {
            Proxy->UpdateLightConstants();
        }
    }
}

void FScene::MarkProxyDirty(FPrimitiveSceneProxy* Proxy, ESceneProxyDirtyFlag Flag)
{
    if (!Proxy)
    {
        return;
    }

    Proxy->MarkDirty(Flag);
    EnqueueDirtyProxy(PrimitiveProxyRegistry.DirtyProxies, Proxy);
}

void FScene::MarkLightProxyDirty(FLightSceneProxy* Proxy, ESceneProxyDirtyFlag Flag)
{
    if (!Proxy)
    {
        return;
    }

    Proxy->MarkDirty(Flag);
    EnqueueDirtyProxy(LightProxyRegistry.DirtyProxies, Proxy);
}

void FScene::MarkAllPerObjectCBDirty()
{
    for (FPrimitiveSceneProxy* Proxy : PrimitiveProxyRegistry.Proxies)
    {
        if (Proxy)
        {
            Proxy->MarkPerObjectCBDirty();
        }
    }
}

void FScene::SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected)
{
    if (!Proxy)
    {
        return;
    }

    Proxy->bSelected = bSelected;

    if (bSelected)
    {
        if (Proxy->SelectedListIndex == UINT32_MAX)
        {
            Proxy->SelectedListIndex = static_cast<uint32>(PrimitiveProxyRegistry.SelectedProxies.size());
            PrimitiveProxyRegistry.SelectedProxies.push_back(Proxy);
        }
    }
    else
    {
        RemoveSelectedProxyFast(PrimitiveProxyRegistry.SelectedProxies, Proxy);
    }
}

bool FScene::IsProxySelected(const FPrimitiveSceneProxy* Proxy) const
{
    return Proxy && Proxy->SelectedListIndex != UINT32_MAX;
}

FLightSceneProxy* FScene::AddLight(ULightComponent* Component)
{
    if (!Component)
    {
        return nullptr;
    }

    FLightSceneProxy* Proxy = Component->CreateLightSceneProxy();
    if (!Proxy)
    {
        return nullptr;
    }

    RegisterLightProxy(Proxy);
    return Proxy;
}

void FScene::RegisterLightProxy(FLightSceneProxy* Proxy)
{
    if (!Proxy)
    {
        return;
    }

    Proxy->DirtyFlags = ESceneProxyDirtyFlag::All;

    if (!LightProxyRegistry.FreeSlots.empty())
    {
        const uint32 Slot = LightProxyRegistry.FreeSlots.back();
        LightProxyRegistry.FreeSlots.pop_back();
        Proxy->ProxyId                   = Slot;
        LightProxyRegistry.Proxies[Slot] = Proxy;
    }
    else
    {
        Proxy->ProxyId = static_cast<uint32>(LightProxyRegistry.Proxies.size());
        LightProxyRegistry.Proxies.push_back(Proxy);
    }

    EnqueueDirtyProxy(LightProxyRegistry.DirtyProxies, Proxy);
}

void FScene::RemoveLight(FLightSceneProxy* Proxy)
{
    if (!Proxy || Proxy->ProxyId == UINT32_MAX)
    {
        return;
    }

    const uint32 Slot = Proxy->ProxyId;

    if (Proxy->bQueuedForDirtyUpdate)
    {
        auto It = std::find(LightProxyRegistry.DirtyProxies.begin(), LightProxyRegistry.DirtyProxies.end(), Proxy);
        if (It != LightProxyRegistry.DirtyProxies.end())
        {
            *It = LightProxyRegistry.DirtyProxies.back();
            LightProxyRegistry.DirtyProxies.pop_back();
        }
        Proxy->bQueuedForDirtyUpdate = false;
    }

    LightProxyRegistry.Proxies[Slot] = nullptr;
    LightProxyRegistry.FreeSlots.push_back(Slot);

    delete Proxy;
}

FFogSceneProxy* FScene::AddFog(const UHeightFogComponent* Owner, const FFogSceneData& FogData)
{
    for (FFogSceneProxy* Proxy : FogProxyRegistry.Proxies)
    {
        if (Proxy && Proxy->GetOwner() == Owner)
        {
            Proxy->UpdateData(FogData);
            return Proxy;
        }
    }

    FFogSceneProxy* Proxy = new FFogSceneProxy(Owner, FogData);
    Proxy->ProxyId        = static_cast<uint32>(FogProxyRegistry.Proxies.size());
    FogProxyRegistry.Proxies.push_back(Proxy);
    return Proxy;
}

void FScene::RemoveFog(const UHeightFogComponent* Owner)
{
    for (uint32 Index = 0; Index < FogProxyRegistry.Proxies.size(); ++Index)
    {
        FFogSceneProxy* Proxy = FogProxyRegistry.Proxies[Index];
        if (!Proxy || Proxy->GetOwner() != Owner)
        {
            continue;
        }

        delete Proxy;
        FogProxyRegistry.Proxies[Index] = nullptr;

        while (!FogProxyRegistry.Proxies.empty() && FogProxyRegistry.Proxies.back() == nullptr)
        {
            FogProxyRegistry.Proxies.pop_back();
        }
        return;
    }
}

bool FScene::HasFog() const
{
    for (FFogSceneProxy* Proxy : FogProxyRegistry.Proxies)
    {
        if (Proxy)
        {
            return true;
        }
    }
    return false;
}

const FFogSceneData& FScene::GetFogData() const
{
    for (FFogSceneProxy* Proxy : FogProxyRegistry.Proxies)
    {
        if (Proxy)
        {
            return Proxy->GetFogData();
        }
    }

    static const FFogSceneData EmptyFogData = {};
    return EmptyFogData;
}
