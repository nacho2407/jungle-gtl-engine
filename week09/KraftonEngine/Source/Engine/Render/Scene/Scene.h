#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Debug/DebugPrimitiveQueue.h"
#include "Render/Scene/Proxies/Effects/FogSceneProxy.h"
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/SceneProxyRegistry.h"

class UPrimitiveComponent;
class ULightComponent;
class UHeightFogComponent;

class FScene
{
public:
    FScene() = default;
    ~FScene();

    FPrimitiveSceneProxy* AddPrimitive(UPrimitiveComponent* Component);
    void                  RegisterPrimitiveProxy(FPrimitiveSceneProxy* Proxy);
    void                  RemovePrimitive(FPrimitiveSceneProxy* Proxy);

    FLightSceneProxy* AddLight(ULightComponent* Component);
    void              RegisterLightProxy(FLightSceneProxy* Proxy);
    void              RemoveLight(FLightSceneProxy* Proxy);

    FFogSceneProxy* AddFog(const UHeightFogComponent* Owner, const FFogSceneData& FogData);
    void            RemoveFog(const UHeightFogComponent* Owner);

    void UpdateDirtyProxies();
    void UpdateDirtyLightProxies();
    void MarkProxyDirty(FPrimitiveSceneProxy* Proxy, ESceneProxyDirtyFlag Flag);
    void MarkLightProxyDirty(FLightSceneProxy* Proxy, ESceneProxyDirtyFlag Flag);
    void MarkAllPerObjectCBDirty();
    void ClearFrameData() {}

    void SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected);
    bool IsProxySelected(const FPrimitiveSceneProxy* Proxy) const;

    const TArray<FPrimitiveSceneProxy*>& GetPrimitiveProxies() const { return PrimitiveProxyRegistry.Proxies; }
    const TArray<FPrimitiveSceneProxy*>& GetNeverCullProxies() const { return PrimitiveProxyRegistry.NeverCullProxies; }
    const TArray<FLightSceneProxy*>&     GetLightProxies() const { return LightProxyRegistry.Proxies; }
    const TArray<FFogSceneProxy*>&       GetFogProxies() const { return FogProxyRegistry.Proxies; }
    uint32                               GetPrimitiveProxyCount() const { return static_cast<uint32>(PrimitiveProxyRegistry.Proxies.size()); }
    uint32                               GetLightProxyCount() const { return static_cast<uint32>(LightProxyRegistry.Proxies.size()); }
    uint32                               GetProxyCount() const { return GetPrimitiveProxyCount() + GetLightProxyCount(); }

    FDebugPrimitiveQueue&       GetDebugPrimitiveQueue() { return DebugPrimitiveQueue; }
    const FDebugPrimitiveQueue& GetDebugPrimitiveQueue() const { return DebugPrimitiveQueue; }

    bool              HasFog() const;
    const FFogSceneData& GetFogData() const;

private:
    FPrimitiveSceneProxyRegistry          PrimitiveProxyRegistry;
    TSceneProxyRegistry<FLightSceneProxy> LightProxyRegistry;
    TSceneProxyRegistry<FFogSceneProxy>   FogProxyRegistry;

    FDebugPrimitiveQueue DebugPrimitiveQueue;
};
