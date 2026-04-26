#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Debug/DebugPrimitiveQueue.h"
#include "Render/Scene/Proxies/Effects/FogSceneProxy.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/SceneProxyRegistry.h"

class UPrimitiveComponent;
class ULightComponent;
class UHeightFogComponent;

// FScene는 렌더 영역의 핵심 동작을 담당합니다.
class FScene
{
public:
    FScene() = default;
    ~FScene();

    FPrimitiveProxy* AddPrimitive(UPrimitiveComponent* Component);
    void             RegisterPrimitiveProxy(FPrimitiveProxy* Proxy);
    void             RemovePrimitive(FPrimitiveProxy* Proxy);

    FLightProxy* AddLight(ULightComponent* Component);
    void         RegisterLightProxy(FLightProxy* Proxy);
    void         RemoveLight(FLightProxy* Proxy);

    FFogSceneProxy* AddFog(const UHeightFogComponent* Owner, const FFogSceneData& FogData);
    void            RemoveFog(const UHeightFogComponent* Owner);

    void UpdateDirtyProxies();
    void UpdateDirtyLightProxies();
    void MarkProxyDirty(FPrimitiveProxy* Proxy, ESceneProxyDirtyFlag Flag);
    void MarkLightProxyDirty(FLightProxy* Proxy, ESceneProxyDirtyFlag Flag);
    void MarkAllPerObjectCBDirty();
    void ClearFrameData() {}

    void SetProxySelected(FPrimitiveProxy* Proxy, bool bSelected);
    bool IsProxySelected(const FPrimitiveProxy* Proxy) const;

    const TArray<FPrimitiveProxy*>& GetPrimitiveProxies() const { return PrimitiveProxyRegistry.Proxies; }
    const TArray<FPrimitiveProxy*>& GetNeverCullProxies() const { return PrimitiveProxyRegistry.NeverCullProxies; }
    const TArray<FLightProxy*>&     GetLightProxies() const { return LightProxyRegistry.Proxies; }
    const TArray<FFogSceneProxy*>&  GetFogProxies() const { return FogProxyRegistry.Proxies; }
    uint32                          GetPrimitiveProxyCount() const { return static_cast<uint32>(PrimitiveProxyRegistry.Proxies.size()); }
    uint32                          GetLightProxyCount() const { return static_cast<uint32>(LightProxyRegistry.Proxies.size()); }
    uint32                          GetProxyCount() const { return GetPrimitiveProxyCount() + GetLightProxyCount(); }

    FDebugPrimitiveQueue&       GetDebugPrimitiveQueue() { return DebugPrimitiveQueue; }
    const FDebugPrimitiveQueue& GetDebugPrimitiveQueue() const { return DebugPrimitiveQueue; }

    bool                 HasFog() const;
    const FFogSceneData& GetFogData() const;

private:
    FPrimitiveProxyRegistry             PrimitiveProxyRegistry;
    TSceneProxyRegistry<FLightProxy>    LightProxyRegistry;
    TSceneProxyRegistry<FFogSceneProxy> FogProxyRegistry;

    FDebugPrimitiveQueue DebugPrimitiveQueue;
};
