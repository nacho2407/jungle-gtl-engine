#include "Render/Submission/Atlas/LightShadowAllocationRegistry.h"

#include "Core/Logging/LogMacros.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

#include <algorithm>

#include "Render/Resources/Shadows/ShadowMapSettings.h"

namespace
{
const char* GetLightTypeName(uint32 LightType)
{
    switch (static_cast<ELightType>(LightType))
    {
    case ELightType::Ambient:
        return "Ambient";
    case ELightType::Directional:
        return "Directional";
    case ELightType::Point:
        return "Point";
    case ELightType::Spot:
        return "Spot";
    default:
        return "Unknown";
    }
}

FString GetLightLabel(const FLightProxy& Light)
{
    return (Light.Owner != nullptr) ? Light.Owner->GetFName().ToString() : FString("UnnamedLight");
}

FString FormatShadowAllocation(const FShadowMapData& Allocation)
{
    return "page=" + std::to_string(Allocation.AtlasPageIndex) +
           " slice=" + std::to_string(Allocation.SliceIndex) +
           " rect=(" + std::to_string(Allocation.ViewportRect.X) +
           ", " + std::to_string(Allocation.ViewportRect.Y) +
           " " + std::to_string(Allocation.ViewportRect.Width) +
           "x" + std::to_string(Allocation.ViewportRect.Height) + ")";
}
}

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
    static bool bLoggedShadowRemoval = false;
    static bool bLoggedApplyWithoutReallocation = false;

    if (!Light.Owner || !Light.bCastShadow)
    {
        if (!bLoggedShadowRemoval)
        {
            UE_LOG(Render, Verbose, "Removing shadow allocation for light %s because owner was missing or shadows were disabled.", GetLightLabel(Light).c_str());
            bLoggedShadowRemoval = true;
        }
        RemoveLight(&Light, AtlasPool);
        Light.ClearShadowData();
        return false;
    }

    const uint32 Resolution   = GetShadowResolutionValue(RoundShadowResolutionToTierPolicy(Light.ShadowResolution));
    const uint32 MaxCascades = GetShadowMapMethod() == EShadowMapMethod::Cascade ? ShadowAtlas::MaxCascades : 1u;
    const uint32 CascadeCount = std::clamp(Light.GetCascadeCountSetting(), 1u, MaxCascades);
    const uint32 LightType    = Light.LightProxyInfo.LightType;

    FLightShadowRecord& Record = Records[&Light];
    const bool bNeedsReallocation =
        Record.Resolution != Resolution ||
        Record.CascadeCount != CascadeCount ||
        Record.LightType != LightType;

    if (bNeedsReallocation)
    {
        UE_LOG(Render, Info, "Reallocating %s shadow maps for light %s. Resolution=%u Cascades=%u",
            GetLightTypeName(LightType), GetLightLabel(Light).c_str(), Resolution, CascadeCount);
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
            UE_LOG(Render, Warning, "Failed to allocate %s shadow maps for light %s. Resolution=%u Cascades=%u",
                GetLightTypeName(LightType), GetLightLabel(Light).c_str(), Resolution, CascadeCount);
            Light.ClearShadowData();
            return false;
        }

        bLoggedApplyWithoutReallocation = false;
    }

    SyncLightShadowMatrices(Record, Light);
    Light.ApplyShadowRecord(Record);
    if (!bNeedsReallocation && !bLoggedApplyWithoutReallocation)
    {
        UE_LOG(Render, Verbose, "Applied shadow allocation for light %s without reallocation. Repeated frame logs are now suppressed.", GetLightLabel(Light).c_str());
        bLoggedApplyWithoutReallocation = true;
    }
    return true;
}

void FLightShadowAllocationRegistry::FreeRecord(FLightShadowRecord& Record, FShadowAtlasPool& AtlasPool)
{
    for (FShadowMapData& Cascade : Record.CascadeShadowMapData.Cascades)
    {
        if (Cascade.bAllocated)
        {
            UE_LOG(Render, Verbose, "Releasing directional shadow allocation: %s", FormatShadowAllocation(Cascade).c_str());
        }
        AtlasPool.Free(Cascade);
        Cascade.Reset();
    }

    if (Record.SpotShadowMapData.bAllocated)
    {
        UE_LOG(Render, Verbose, "Releasing spot shadow allocation: %s", FormatShadowAllocation(Record.SpotShadowMapData).c_str());
    }
    AtlasPool.Free(Record.SpotShadowMapData);
    Record.SpotShadowMapData.Reset();

    for (FShadowMapData& Face : Record.CubeShadowMapData.Faces)
    {
        if (Face.bAllocated)
        {
            UE_LOG(Render, Verbose, "Releasing point shadow face allocation: %s", FormatShadowAllocation(Face).c_str());
        }
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
        const FCascadeShadowMapData* LiveCascadeShadowMapData = Light.GetCascadeShadowMapData();
        if (!LiveCascadeShadowMapData)
        {
            return;
        }

        const uint32 CascadeCount = std::min(
            Record.CascadeShadowMapData.CascadeCount,
            static_cast<uint32>(ShadowAtlas::MaxCascades));
        for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
        {
            Record.CascadeShadowMapData.CascadeViews[CascadeIndex] = LiveCascadeShadowMapData->CascadeViews[CascadeIndex];
            Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = LiveCascadeShadowMapData->CascadeViewProj[CascadeIndex];
        }
        for (uint32 SplitIndex = 0; SplitIndex <= CascadeCount; ++SplitIndex)
        {
            Record.CascadeShadowMapData.CascadeSplits[SplitIndex] = LiveCascadeShadowMapData->CascadeSplits[SplitIndex];
        }
        return;
    }

    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        const FCubeShadowMapData* LiveCubeShadowMapData = Light.GetCubeShadowMapData();
        const FMatrix* PointShadowViewProjMatrices = Light.GetPointShadowViewProjMatrices();
        if (!LiveCubeShadowMapData || !PointShadowViewProjMatrices)
        {
            return;
        }

        for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
        {
            Record.CubeShadowMapData.FaceViews[FaceIndex] = LiveCubeShadowMapData->FaceViews[FaceIndex];
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
            UE_LOG(Render, Warning, "Directional shadow cascade allocation failed for light %s at cascade %u.", GetLightLabel(Light).c_str(), CascadeIndex);
            FreeRecord(Record, AtlasPool);
            return false;
        }
        Record.CascadeShadowMapData.CascadeViewProj[CascadeIndex] = Light.LightViewProj;
        UE_LOG(Render, Info, "Directional shadow cascade %u allocated for %s: %s",
            CascadeIndex, GetLightLabel(Light).c_str(), FormatShadowAllocation(Record.CascadeShadowMapData.Cascades[CascadeIndex]).c_str());
    }

    const FCascadeShadowMapData* LiveCascadeShadowMapData = Light.GetCascadeShadowMapData();
    if (LiveCascadeShadowMapData)
    {
        for (uint32 SplitIndex = 0; SplitIndex <= Record.CascadeCount; ++SplitIndex)
        {
            Record.CascadeShadowMapData.CascadeSplits[SplitIndex] = LiveCascadeShadowMapData->CascadeSplits[SplitIndex];
        }
    }

    return true;
}

bool FLightShadowAllocationRegistry::AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    Record.SpotShadowMapData.Reset();
    const bool bAllocated = AtlasPool.Allocate(Device, Record.Resolution, Record.SpotShadowMapData);
    if (bAllocated)
    {
        UE_LOG(Render, Info, "Spot shadow allocated for %s: %s", GetLightLabel(Light).c_str(), FormatShadowAllocation(Record.SpotShadowMapData).c_str());
    }
    else
    {
        UE_LOG(Render, Warning, "Spot shadow allocation failed for light %s. Resolution=%u", GetLightLabel(Light).c_str(), Record.Resolution);
    }
    return bAllocated;
}

bool FLightShadowAllocationRegistry::AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool)
{
    Record.CubeShadowMapData.Reset();
    const FMatrix* PointShadowViewProjMatrices = Light.GetPointShadowViewProjMatrices();
    if (!PointShadowViewProjMatrices)
    {
        UE_LOG(Render, Warning, "Point light %s had no view-projection matrices for shadow allocation.", GetLightLabel(Light).c_str());
        return false;
    }

    for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
    {
        if (!AtlasPool.Allocate(Device, Record.Resolution, Record.CubeShadowMapData.Faces[FaceIndex]))
        {
            UE_LOG(Render, Warning, "Point shadow face allocation failed for light %s at face %u.", GetLightLabel(Light).c_str(), FaceIndex);
            FreeRecord(Record, AtlasPool);
            return false;
        }
        Record.CubeShadowMapData.FaceViewProj[FaceIndex] = PointShadowViewProjMatrices[FaceIndex];
        UE_LOG(Render, Info, "Point shadow face %u allocated for %s: %s",
            FaceIndex, GetLightLabel(Light).c_str(), FormatShadowAllocation(Record.CubeShadowMapData.Faces[FaceIndex]).c_str());
    }
    return true;
}
