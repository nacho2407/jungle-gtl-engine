#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Render/Types/FogParams.h"
#include "Render/Scene/DebugDraw/SceneDebugData.h"

class UPrimitiveComponent;
class ULightComponent;

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

    void UpdateDirtyProxies();
    void UpdateDirtyLightProxies();
    void MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag);
    void MarkLightProxyDirty(FLightSceneProxy* Proxy, EDirtyFlag Flag);
    void MarkAllPerObjectCBDirty();

    void SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected);
    bool IsProxySelected(const FPrimitiveSceneProxy* Proxy) const;

    const TArray<FPrimitiveSceneProxy*>& GetPrimitiveProxies() const { return Proxies; }
    const TArray<FPrimitiveSceneProxy*>& GetNeverCullProxies() const { return NeverCullProxies; }
    const TArray<FLightSceneProxy*>& GetLightProxies() const { return LightProxies; }
    uint32 GetPrimitiveProxyCount() const { return static_cast<uint32>(Proxies.size()); }
    uint32 GetLightProxyCount() const { return static_cast<uint32>(LightProxies.size()); }
    uint32 GetProxyCount() const { return GetPrimitiveProxyCount() + GetLightProxyCount(); }

    void ClearFrameData();

    void AddOverlayText(FString Text, const FVector2& Position, float Scale) { DebugData.AddOverlayText(std::move(Text), Position, Scale); }
    const TArray<FSceneOverlayText>& GetOverlayTexts() const { return DebugData.GetOverlayTexts(); }

    void AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color) { DebugData.AddDebugAABB(Min, Max, Color); }
    const TArray<FSceneDebugAABB>& GetDebugAABBs() const { return DebugData.GetDebugAABBs(); }

    void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color) { DebugData.AddDebugLine(Start, End, Color); }
    const TArray<FSceneDebugLine>& GetDebugLines() const { return DebugData.GetDebugLines(); }

    void SetGrid(float Spacing, int32 HalfLineCount) { DebugData.SetGrid(Spacing, HalfLineCount); }
    bool HasGrid() const { return DebugData.HasGrid(); }
    float GetGridSpacing() const { return DebugData.GetGridSpacing(); }
    int32 GetGridHalfLineCount() const { return DebugData.GetGridHalfLineCount(); }

    FDebugDrawQueue& GetDebugDrawQueue() { return DebugData.GetDebugDrawQueue(); }
    const FDebugDrawQueue& GetDebugDrawQueue() const { return DebugData.GetDebugDrawQueue(); }
    FSceneDebugData& GetDebugData() { return DebugData; }
    const FSceneDebugData& GetDebugData() const { return DebugData; }

    void AddFog(const class UHeightFogComponent* Owner, const FFogParams& Params);
    void RemoveFog(const class UHeightFogComponent* Owner);
    bool HasFog() const { return !Fogs.empty(); }
    const FFogParams& GetFogParams() const { return Fogs[0].Params; }

private:
    TArray<FPrimitiveSceneProxy*> Proxies;
    TArray<FPrimitiveSceneProxy*> DirtyProxies;
    TArray<FPrimitiveSceneProxy*> SelectedProxies;
    TArray<FPrimitiveSceneProxy*> NeverCullProxies;
    TArray<uint32> FreeSlots;

    TArray<FLightSceneProxy*> LightProxies;
    TArray<FLightSceneProxy*> DirtyLightProxies;
    TArray<uint32> FreeLightSlots;

    FSceneDebugData DebugData;

    struct FFogEntry
    {
        const class UHeightFogComponent* Owner = nullptr;
        FFogParams Params;
    };
    TArray<FFogEntry> Fogs;
};
