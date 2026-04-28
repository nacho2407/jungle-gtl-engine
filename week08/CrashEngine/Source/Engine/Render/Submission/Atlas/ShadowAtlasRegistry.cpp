#include "Render/Submission/Atlas/ShadowAtlasRegistry.h"

#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Render/Resources/Shadows/ShadowResolutionSettings.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"

#include <algorithm>

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

    const uint32 Resolution = GetShadowResolutionValue(RoundShadowResolutionToTier(Light.ShadowResolution));
    // 수정: 현재 프레임의 CascadeShadowMapData.CascadeCount를 사용하여 렌더 설정(CSM 여부)에 따른 할당을 수행합니다.
    const FCascadeShadowMapData* CascadeData = Light.GetCascadeShadowMapData();
    const uint32 CascadeCount = CascadeData ? std::clamp(CascadeData->CascadeCount, 1u, static_cast<uint32>(ShadowAtlas::MaxCascades)) : 1u;
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
        const FCascadeShadowMapData* SrcCascadeData = Light.GetCascadeShadowMapData();
        if (!SrcCascadeData) return;

        const uint32 CascadeCount = Record.CascadeCount;
        Record.CascadeShadowMapData.CascadeCount = CascadeCount;
        for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
        {
            Record.CascadeShadowMapData.CascadeViews[CascadeIndex] = SrcCascadeData->CascadeViews[CascadeIndex];
            Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = SrcCascadeData->CascadeViewProj[CascadeIndex];
        }
        for (uint32 SplitIndex = 0; SplitIndex <= CascadeCount; ++SplitIndex)
        {
            Record.CascadeShadowMapData.CascadeSplits[SplitIndex] = SrcCascadeData->CascadeSplits[SplitIndex];
        }
        return;
    }

    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        const FCubeShadowMapData* SrcCubeData = Light.GetCubeShadowMapData();
        if (!SrcCubeData) return;

        for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
        {
            Record.CubeShadowMapData.FaceViews[FaceIndex] = SrcCubeData->FaceViews[FaceIndex];
            Record.CubeShadowMapData.FaceViewProj[FaceIndex] = SrcCubeData->FaceViewProj[FaceIndex];
        }
        return;
    }
}

bool FShadowAtlasRegistry::AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager)
{
    Record.CascadeShadowMapData.Reset();
    Record.CascadeShadowMapData.CascadeCount = Record.CascadeCount;

    for (uint32 CascadeIndex = 0; CascadeIndex < Record.CascadeCount; ++CascadeIndex)
    {
        if (!AtlasManager.Allocate(Device, Record.Resolution, Record.CascadeShadowMapData.Cascades[CascadeIndex]))
        {
            FreeRecord(Record, AtlasManager);
            return false;
        }

        const FCascadeShadowMapData* SrcCascadeData = Light.GetCascadeShadowMapData();
        if (SrcCascadeData)
        {
            Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = SrcCascadeData->CascadeViewProj[CascadeIndex];
            Record.CascadeShadowMapData.CascadeViews[CascadeIndex] = SrcCascadeData->CascadeViews[CascadeIndex];
        }
    }

    const FCascadeShadowMapData* SrcCascadeData = Light.GetCascadeShadowMapData();
    if (SrcCascadeData)
    {
        for (uint32 SplitIndex = 0; SplitIndex <= Record.CascadeCount; ++SplitIndex)
        {
            Record.CascadeShadowMapData.CascadeSplits[SplitIndex] = SrcCascadeData->CascadeSplits[SplitIndex];
        }
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
    const FMatrix* PointShadowViewProjMatrices = Light.GetPointShadowViewProjMatrices();
    const FCubeShadowMapData* SrcCubeData = Light.GetCubeShadowMapData();

    for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
    {
        if (!AtlasManager.Allocate(Device, Record.Resolution, Record.CubeShadowMapData.Faces[FaceIndex]))
        {
            FreeRecord(Record, AtlasManager);
            return false;
        }
        
        if (PointShadowViewProjMatrices)
        {
            Record.CubeShadowMapData.FaceViewProj[FaceIndex] = PointShadowViewProjMatrices[FaceIndex];
        }
        if (SrcCubeData)
        {
            Record.CubeShadowMapData.FaceViews[FaceIndex] = SrcCubeData->FaceViews[FaceIndex];
        }
    }
    return true;
}
