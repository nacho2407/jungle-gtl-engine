#pragma once

#include "Core/CoreTypes.h"
#include "Render/Passes/Scene/FogParams.h"
#include "Render/Scene/DebugDraw/DebugDrawQueue.h"
#include "Render/Scene/Proxies/Effects/FogSceneProxy.h"
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/SceneProxyRegistry.h"

class UPrimitiveComponent;
class ULightComponent;
class UHeightFogComponent;

/*
    FScene는 렌더용 프록시 registry를 관리하는 컨테이너입니다.
    Primitive / Light / Effect 프록시의 등록, dirty 추적, 선택 상태, free slot 재사용을 담당합니다.
*/
class FScene
{
public:
    FScene() = default;
    ~FScene();

    FPrimitiveSceneProxy* AddPrimitive(UPrimitiveComponent* Component);
    void RegisterPrimitiveProxy(FPrimitiveSceneProxy* Proxy);
    void RemovePrimitive(FPrimitiveSceneProxy* Proxy);

    FLightSceneProxy* AddLight(ULightComponent* Component);
    void RegisterLightProxy(FLightSceneProxy* Proxy);
    void RemoveLight(FLightSceneProxy* Proxy);

    FFogSceneProxy* AddFog(const UHeightFogComponent* Owner, const FFogParams& Params);
    void RemoveFog(const UHeightFogComponent* Owner);

    void UpdateDirtyProxies();
    void UpdateDirtyLightProxies();
    void MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag);
    void MarkLightProxyDirty(FLightSceneProxy* Proxy, EDirtyFlag Flag);
    void MarkAllPerObjectCBDirty();

    void SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected);
    bool IsProxySelected(const FPrimitiveSceneProxy* Proxy) const;

    const TArray<FPrimitiveSceneProxy*>& GetPrimitiveProxies() const { return PrimitiveProxyRegistry.Proxies; }
    const TArray<FPrimitiveSceneProxy*>& GetNeverCullProxies() const { return PrimitiveProxyRegistry.NeverCullProxies; }
    const TArray<FLightSceneProxy*>& GetLightProxies() const { return LightProxyRegistry.Proxies; }
    const TArray<FFogSceneProxy*>& GetFogProxies() const { return FogProxyRegistry.Proxies; }
    uint32 GetPrimitiveProxyCount() const { return static_cast<uint32>(PrimitiveProxyRegistry.Proxies.size()); }
    uint32 GetLightProxyCount() const { return static_cast<uint32>(LightProxyRegistry.Proxies.size()); }
    uint32 GetProxyCount() const { return GetPrimitiveProxyCount() + GetLightProxyCount(); }

    void ClearFrameData() { ImmediateDebugLines.clear(); }

    FDebugDrawQueue& GetDebugDrawQueue() { return DebugDrawQueue; }
    const FDebugDrawQueue& GetDebugDrawQueue() const { return DebugDrawQueue; }
    const TArray<FDebugDrawItem>& GetImmediateDebugLines() const { return ImmediateDebugLines; }
    void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color)
    {
        FDebugDrawItem Item;
        Item.Start = Start;
        Item.End = End;
        Item.Color = Color;
        Item.RemainingTime = 0.0f;
        Item.bOneFrame = true;
        ImmediateDebugLines.push_back(Item);
    }

    bool HasFog() const;
    const FFogParams& GetFogParams() const;

private:
    FPrimitiveSceneProxyRegistry PrimitiveProxyRegistry;
    TSceneProxyRegistry<FLightSceneProxy> LightProxyRegistry;
    TSceneProxyRegistry<FFogSceneProxy> FogProxyRegistry;

    FDebugDrawQueue DebugDrawQueue;
    TArray<FDebugDrawItem> ImmediateDebugLines;
};
