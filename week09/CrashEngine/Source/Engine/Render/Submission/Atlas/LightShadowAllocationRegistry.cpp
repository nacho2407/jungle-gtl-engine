#include "Render/Submission/Atlas/LightShadowAllocationRegistry.h"

#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

#include <algorithm>

void FLightShadowAllocationRegistry::Release(FShadowAtlasPool& AtlasPool)
{
    for (auto& Pair : Records)
    {
        FreeRecord(Pair.second, AtlasPool);
    }
    Records.clear();
}

void FLightShadowAllocationRegistry::RemoveLight(FLightProxy* Light, FShadowAtlasPool& AtlasPool)
{
    auto It = Records.find(Light);
    if (It == Records.end())
    {
        return;
    }

    FreeRecord(It->second, AtlasPool);
    Records.erase(It);
}

bool FLightShadowAllocationRegistry::UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    if (!Light.Owner || !Light.bCastShadow)
    {
        RemoveLight(&Light, AtlasPool);
        Light.ClearShadowData();
        return false;
    }

    const uint32 Resolution   = RoundShadowResolutionToTier(Light.ShadowResolution);
    const uint32 CascadeCount = std::clamp(Light.GetCascadeCountSetting(), 1, static_cast<int32>(ShadowAtlas::MaxCascades));
    const uint32 LightType    = Light.LightProxyInfo.LightType;

    FLightShadowRecord& Record = Records[&Light];
    const bool bNeedsReallocation =
        Record.Resolution != Resolution ||
        Record.CascadeCount != CascadeCount ||
        Record.LightType != LightType;

    if (bNeedsReallocation)
    {
        FreeRecord(Record, AtlasPool);
        Record.Resolution   = Resolution;
        Record.CascadeCount = CascadeCount;
        Record.LightType    = LightType;

        bool bAllocated = false;
        if (LightType == static_cast<uint32>(ELightType::Directional))
        {
            bAllocated = AllocateDirectional(Record, Light, Device, AtlasPool);
        }
        else if (LightType == static_cast<uint32>(ELightType::Point))
        {
            bAllocated = AllocatePoint(Record, Light, Device, AtlasPool);
        }
        else if (LightType == static_cast<uint32>(ELightType::Spot))
        {
            bAllocated = AllocateSpot(Record, Light, Device, AtlasPool);
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

void FLightShadowAllocationRegistry::FreeRecord(FLightShadowRecord& Record, FShadowAtlasPool& AtlasPool)
{
    for (FShadowMapData& Cascade : Record.CascadeShadowMapData.Cascades)
    {
        AtlasPool.Free(Cascade);
        Cascade.Reset();
    }

    AtlasPool.Free(Record.SpotShadowMapData);
    Record.SpotShadowMapData.Reset();

    for (FShadowMapData& Face : Record.CubeShadowMapData.Faces)
    {
        AtlasPool.Free(Face);
        Face.Reset();
    }

    Record.CascadeShadowMapData.Reset();
    Record.CubeShadowMapData.Reset();
}

void FLightShadowAllocationRegistry::SyncLightShadowMatrices(FLightShadowRecord& Record, const FLightProxy& Light) const
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
        const FMatrix* PointShadowViewProjMatrices = Light.GetPointShadowViewProjMatrices();
        if (!PointShadowViewProjMatrices)
        {
            return;
        }

        for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
        {
            Record.CubeShadowMapData.FaceViewProj[FaceIndex] = PointShadowViewProjMatrices[FaceIndex];
        }
    }
}

bool FLightShadowAllocationRegistry::AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    Record.CascadeShadowMapData.Reset();
    Record.CascadeShadowMapData.CascadeCount = Record.CascadeCount;

    for (uint32 CascadeIndex = 0; CascadeIndex < Record.CascadeCount; ++CascadeIndex)
    {
        if (!AtlasPool.Allocate(Device, Record.Resolution, Record.CascadeShadowMapData.Cascades[CascadeIndex]))
        {
            FreeRecord(Record, AtlasPool);
            return false;
        }
        Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = Light.LightViewProj;
    }

    return true;
}

bool FLightShadowAllocationRegistry::AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    (void)Light;
    Record.SpotShadowMapData.Reset();
    return AtlasPool.Allocate(Device, Record.Resolution, Record.SpotShadowMapData);
}

bool FLightShadowAllocationRegistry::AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    Record.CubeShadowMapData.Reset();
    const FMatrix* PointShadowViewProjMatrices = Light.GetPointShadowViewProjMatrices();
    if (!PointShadowViewProjMatrices)
    {
        return false;
    }

    for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
    {
        if (!AtlasPool.Allocate(Device, Record.Resolution, Record.CubeShadowMapData.Faces[FaceIndex]))
        {
            FreeRecord(Record, AtlasPool);
            return false;
        }
        Record.CubeShadowMapData.FaceViewProj[FaceIndex] = PointShadowViewProjMatrices[FaceIndex];
    }
    return true;
}
