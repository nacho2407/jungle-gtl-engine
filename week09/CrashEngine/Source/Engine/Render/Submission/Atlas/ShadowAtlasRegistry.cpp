#include "Render/Submission/Atlas/ShadowAtlasRegistry.h"

#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"

#include <algorithm>

namespace
{
uint32 ClampShadowResolution(uint32 Resolution)
{
    if (Resolution <= 256u) return 256u;
    if (Resolution <= 512u) return 512u;
    if (Resolution <= 1024u) return 1024u;
    if (Resolution <= 2048u) return 2048u;
    return 4096u;
}
} // namespace

void FShadowAtlasRegistry::Release(FShadowAtlasManager& AtlasManager)
{
    for (auto& Pair : Records)
    {
        FreeRecord(Pair.second, AtlasManager);
    }
    Records.clear();
}

void FShadowAtlasRegistry::RemoveLight(FLightProxy* Light, FShadowAtlasManager& AtlasManager)
{
    auto It = Records.find(Light);
    if (It == Records.end())
    {
        return;
    }

    FreeRecord(It->second, AtlasManager);
    Records.erase(It);
}

bool FShadowAtlasRegistry::UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager)
{
    if (!Light.Owner || !Light.bCastShadow)
    {
        RemoveLight(&Light, AtlasManager);
        Light.ClearShadowData();
        return false;
    }

    const uint32 Resolution = ClampShadowResolution(Light.ShadowResolution);
    const uint32 CascadeCount = std::clamp(Light.CascadeCount, 1, static_cast<int32>(ShadowAtlas::MaxCascades));
    const uint32 LightType = Light.LightProxyInfo.LightType;

    FLightShadowRecord& Record = Records[&Light];
    const bool bNeedsReallocation =
        Record.Resolution != Resolution ||
        Record.CascadeCount != CascadeCount ||
        Record.LightType != LightType;

    if (bNeedsReallocation)
    {
        FreeRecord(Record, AtlasManager);
        Record.Resolution = Resolution;
        Record.CascadeCount = CascadeCount;
        Record.LightType = LightType;

        bool bAllocated = false;
        if (LightType == static_cast<uint32>(ELightType::Directional))
        {
            bAllocated = AllocateDirectional(Record, Light, Device, AtlasManager);
        }
        else if (LightType == static_cast<uint32>(ELightType::Point))
        {
            bAllocated = AllocatePoint(Record, Light, Device, AtlasManager);
        }
        else if (LightType == static_cast<uint32>(ELightType::Spot))
        {
            bAllocated = AllocateSpot(Record, Light, Device, AtlasManager);
        }

        if (!bAllocated)
        {
            Light.ClearShadowData();
            return false;
        }
    }

    SyncLightShadowMatrices(Record, Light);
    Light.ApplyShadowRecord(Record);
    return true;
}

void FShadowAtlasRegistry::FreeRecord(FLightShadowRecord& Record, FShadowAtlasManager& AtlasManager)
{
    for (FShadowMapData& Cascade : Record.CascadeShadowMapData.Cascades)
    {
        AtlasManager.Free(Cascade);
        Cascade.Reset();
    }

    AtlasManager.Free(Record.SpotShadowMapData);
    Record.SpotShadowMapData.Reset();

    for (FShadowMapData& Face : Record.CubeShadowMapData.Faces)
    {
        AtlasManager.Free(Face);
        Face.Reset();
    }

    Record.CascadeShadowMapData.Reset();
    Record.CubeShadowMapData.Reset();
}

void FShadowAtlasRegistry::SyncLightShadowMatrices(FLightShadowRecord& Record, const FLightProxy& Light) const
{
    const uint32 LightType = Light.LightProxyInfo.LightType;
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        const uint32 CascadeCount = std::min(
            Record.CascadeShadowMapData.CascadeCount,
            static_cast<uint32>(ShadowAtlas::MaxCascades));
        for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
        {
            Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = Light.LightViewProj;
        }
        return;
    }

    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
        {
            Record.CubeShadowMapData.FaceViewProj[FaceIndex] = Light.ShadowViewProjMatrices[FaceIndex];
        }
        return;
    }

}

bool FShadowAtlasRegistry::AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager)
{
    Record.CascadeShadowMapData.Reset();
    Record.CascadeShadowMapData.CascadeCount = Record.CascadeCount;

    // 현재는 cascade별 atlas rect만 따로 잡고, view-proj는 기존 light 계산값을 그대로 사용합니다.
    for (uint32 CascadeIndex = 0; CascadeIndex < Record.CascadeCount; ++CascadeIndex)
    {
        if (!AtlasManager.Allocate(Device, Record.Resolution, Record.CascadeShadowMapData.Cascades[CascadeIndex]))
        {
            FreeRecord(Record, AtlasManager);
            return false;
        }
        Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = Light.LightViewProj;
    }

    return true;
}

bool FShadowAtlasRegistry::AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager)
{
    (void)Light;
    Record.SpotShadowMapData.Reset();
    return AtlasManager.Allocate(Device, Record.Resolution, Record.SpotShadowMapData);
}

bool FShadowAtlasRegistry::AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager)
{
    Record.CubeShadowMapData.Reset();
    for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
    {
        if (!AtlasManager.Allocate(Device, Record.Resolution, Record.CubeShadowMapData.Faces[FaceIndex]))
        {
            FreeRecord(Record, AtlasManager);
            return false;
        }
        Record.CubeShadowMapData.FaceViewProj[FaceIndex] = Light.ShadowViewProjMatrices[FaceIndex];
    }
    return true;
}
