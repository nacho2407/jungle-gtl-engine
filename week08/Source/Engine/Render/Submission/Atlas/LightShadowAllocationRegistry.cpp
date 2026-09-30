#include "Render/Submission/Atlas/LightShadowAllocationRegistry.h"

#include "Core/Logging/LogMacros.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"

#include <algorithm>
#include <cmath>

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

bool HasAllocatedShadowData(const FLightShadowRecord& Record)
{
    for (const FShadowMapData& Cascade : Record.CascadeShadowMapData.Cascades)
    {
        if (Cascade.bAllocated)
        {
            return true;
        }
    }

    if (Record.SpotShadowMapData.bAllocated)
    {
        return true;
    }

    for (const FShadowMapData& Face : Record.CubeShadowMapData.Faces)
    {
        if (Face.bAllocated)
        {
            return true;
        }
    }

    return false;
}

TArray<uint32> BuildResolutionCandidates(uint32 LightType, uint32 RequestedResolution)
{
    TArray<uint32> Candidates;
    auto PushUnique = [&Candidates](uint32 Resolution)
    {
        Resolution = std::clamp(Resolution, ShadowAtlas::MinResolution, ShadowAtlas::MaxResolution);
        if (std::find(Candidates.begin(), Candidates.end(), Resolution) == Candidates.end())
        {
            Candidates.push_back(Resolution);
        }
    };

    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        PushUnique(std::min(RequestedResolution, 1024u));
        PushUnique(512u);
        PushUnique(256u);
    }
    else if (LightType == static_cast<uint32>(ELightType::Spot))
    {
        PushUnique(std::min(RequestedResolution, 2048u));
        PushUnique(1024u);
        PushUnique(512u);
        PushUnique(256u);
    }
    else
    {
        PushUnique(std::min(RequestedResolution, 2048u));
        PushUnique(1024u);
        PushUnique(512u);
    }

    return Candidates;
}

double GetLightTypePriorityWeight(uint32 LightType)
{
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        return 1000.0;
    }
    if (LightType == static_cast<uint32>(ELightType::Spot))
    {
        return 300.0;
    }
    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        return 120.0;
    }
    return 0.0;
}

uint32 GetLightFaceCount(uint32 LightType, uint32 CascadeCount)
{
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        return std::max(1u, CascadeCount);
    }
    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        return ShadowAtlas::MaxPointFaces;
    }
    return 1u;
}
}

void FLightShadowAllocationRegistry::BeginFrame()
{
    ++CurrentFrame;
    for (auto& Pair : Records)
    {
        Pair.second.bTouchedThisFrame = false;
    }
}

void FLightShadowAllocationRegistry::EndFrame(FShadowAtlasPool& AtlasPool)
{
    for (auto It = Records.begin(); It != Records.end(); )
    {
        if (!It->second.bTouchedThisFrame)
        {
            FreeRecord(It->second, AtlasPool);
            It = Records.erase(It);
            continue;
        }

        ++It;
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
    Record.bTouchedThisFrame = true;
    Record.LastTouchedFrame = CurrentFrame;
    const bool bHasAllocation = HasAllocatedShadowData(Record);
    const bool bNeedsReallocation =
        (!bHasAllocation && Record.LastFailedAllocationFrame != CurrentFrame) ||
        Record.RequestedResolution != Resolution ||
        Record.CascadeCount != CascadeCount ||
        Record.LightType != LightType;

    if (bNeedsReallocation)
    {
        UE_LOG(Render, Info, "Updating %s shadow maps for light %s. RequestedResolution=%u Cascades=%u",
            GetLightTypeName(LightType), GetLightLabel(Light).c_str(), Resolution, CascadeCount);
        FLightShadowRecord CandidateRecord = {};
        const bool bAllocated = TryAllocateRecord(CandidateRecord, Light, Device, AtlasPool, Resolution, CascadeCount, LightType);

        if (bAllocated)
        {
            FreeRecord(Record, AtlasPool);
            Record = CandidateRecord;
            Record.bTouchedThisFrame = true;
            Record.LastTouchedFrame = CurrentFrame;
            Record.LastAllocationFrame = CurrentFrame;
            Record.LastFailedAllocationFrame = 0;
        }
        else
        {
            InitializeEmptyRecord(CandidateRecord, Resolution, 0, CascadeCount, LightType);
            CandidateRecord.bTouchedThisFrame = true;
            CandidateRecord.LastTouchedFrame = CurrentFrame;
            CandidateRecord.LastFailedAllocationFrame = CurrentFrame;
            CandidateRecord.FailedAllocationCount = Record.FailedAllocationCount + 1;
            ++FailedAllocationCount;

            if (!HasAllocatedShadowData(Record))
            {
                Record = CandidateRecord;
            }

            UE_LOG(Render, Warning, "Failed to allocate %s shadow maps for light %s. Keeping existing shadow state if any; lighting will fall back without shadow sampling.",
                GetLightTypeName(LightType), GetLightLabel(Light).c_str());
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

FShadowAtlasBudgetStats FLightShadowAllocationRegistry::GetBudgetStats(const FShadowAtlasPool& AtlasPool) const
{
    FShadowAtlasBudgetStats Stats = AtlasPool.GetBudgetStats();
    Stats.FailedAllocationCount = FailedAllocationCount;
    Stats.EvictedShadowCount = 0;

    for (const auto& Pair : Records)
    {
        const FLightShadowRecord& Record = Pair.second;
        if (HasAllocatedShadowData(Record))
        {
            if (Record.LightType == static_cast<uint32>(ELightType::Directional))
            {
                Stats.AllocatedShadowCount += Record.CascadeShadowMapData.CascadeCount;
                ++Stats.DirectionalShadowLightCount;
            }
            else if (Record.LightType == static_cast<uint32>(ELightType::Point))
            {
                Stats.AllocatedShadowCount += ShadowAtlas::MaxPointFaces;
                ++Stats.PointShadowLightCount;
            }
            else if (Record.LightType == static_cast<uint32>(ELightType::Spot))
            {
                ++Stats.AllocatedShadowCount;
                ++Stats.SpotShadowLightCount;
            }
        }
        else if (Record.FailedAllocationCount > 0)
        {
            ++Stats.EvictedShadowCount;
        }
    }

    return Stats;
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

bool FLightShadowAllocationRegistry::TryAllocateRecord(
    FLightShadowRecord& OutRecord,
    FLightProxy&        Light,
    ID3D11Device*       Device,
    FShadowAtlasPool&   AtlasPool,
    uint32              RequestedResolution,
    uint32              CascadeCount,
    uint32              LightType)
{
    const TArray<uint32> ResolutionCandidates = BuildResolutionCandidates(LightType, RequestedResolution);
    const EShadowAllocationPolicy AllocationPolicy = GetShadowAllocationPolicy();

    if (AllocationPolicy == EShadowAllocationPolicy::PreferDowngrade)
    {
        for (uint32 CandidateResolution : ResolutionCandidates)
        {
            InitializeEmptyRecord(OutRecord, RequestedResolution, CandidateResolution, CascadeCount, LightType);
            if (TryAllocateRecordByType(OutRecord, Light, Device, AtlasPool, LightType))
            {
                if (CandidateResolution != RequestedResolution)
                {
                    UE_LOG(Render, Info, "Shadow resolution downgraded for %s (%s): %u -> %u to stay within atlas budget.",
                        GetLightLabel(Light).c_str(), GetLightTypeName(LightType), RequestedResolution, CandidateResolution);
                }
                return true;
            }

            FreeRecord(OutRecord, AtlasPool);
        }

        for (uint32 CandidateResolution : ResolutionCandidates)
        {
            while (true)
            {
                InitializeEmptyRecord(OutRecord, RequestedResolution, CandidateResolution, CascadeCount, LightType);
                if (TryAllocateRecordByType(OutRecord, Light, Device, AtlasPool, LightType))
                {
                    if (CandidateResolution != RequestedResolution)
                    {
                        UE_LOG(Render, Info, "Shadow resolution downgraded for %s (%s): %u -> %u to stay within atlas budget.",
                            GetLightLabel(Light).c_str(), GetLightTypeName(LightType), RequestedResolution, CandidateResolution);
                    }
                    return true;
                }

                FreeRecord(OutRecord, AtlasPool);
                if (!TryEvictForAllocation(Light, AtlasPool, CandidateResolution, CascadeCount, LightType))
                {
                    break;
                }
            }
        }

        return false;
    }

    for (uint32 CandidateResolution : ResolutionCandidates)
    {
        while (true)
        {
            InitializeEmptyRecord(OutRecord, RequestedResolution, CandidateResolution, CascadeCount, LightType);

            if (TryAllocateRecordByType(OutRecord, Light, Device, AtlasPool, LightType))
            {
                if (CandidateResolution != RequestedResolution)
                {
                    UE_LOG(Render, Info, "Shadow resolution downgraded for %s (%s): %u -> %u to stay within atlas budget.",
                        GetLightLabel(Light).c_str(), GetLightTypeName(LightType), RequestedResolution, CandidateResolution);
                }
                return true;
            }

            FreeRecord(OutRecord, AtlasPool);
            if (AllocationPolicy == EShadowAllocationPolicy::DowngradeOnly)
            {
                break;
            }
            if (!TryEvictForAllocation(Light, AtlasPool, CandidateResolution, CascadeCount, LightType))
            {
                break;
            }
        }

    }

    return false;
}

const FShadowMapData* GetFirstAllocatedShadowData(const FLightShadowRecord& Record)
{
    for (const FShadowMapData& Cascade : Record.CascadeShadowMapData.Cascades)
    {
        if (Cascade.bAllocated)
        {
            return &Cascade;
        }
    }

    if (Record.SpotShadowMapData.bAllocated)
    {
        return &Record.SpotShadowMapData;
    }

    for (const FShadowMapData& Face : Record.CubeShadowMapData.Faces)
    {
        if (Face.bAllocated)
        {
            return &Face;
        }
    }

    return nullptr;
}

bool FLightShadowAllocationRegistry::TryAllocateRecordByType(
    FLightShadowRecord& Record,
    FLightProxy&        Light,
    ID3D11Device*       Device,
    FShadowAtlasPool&   AtlasPool,
    uint32              LightType)
{
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        return AllocateDirectional(Record, Light, Device, AtlasPool);
    }
    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        return AllocatePoint(Record, Light, Device, AtlasPool);
    }
    if (LightType == static_cast<uint32>(ELightType::Spot))
    {
        return AllocateSpot(Record, Light, Device, AtlasPool);
    }
    return false;
}

bool FLightShadowAllocationRegistry::TryEvictForAllocation(
    FLightProxy&      RequestLight,
    FShadowAtlasPool& AtlasPool,
    uint32            RequestedResolution,
    uint32            CascadeCount,
    uint32            LightType)
{
    const double RequestScore = ComputeFinalScore(RequestLight, RequestedResolution, CascadeCount, LightType);
    TArray<FShadowEvictionCandidate> Candidates;
    Candidates.reserve(Records.size());

    for (auto& Pair : Records)
    {
        FLightProxy* CandidateLight = Pair.first;
        FLightShadowRecord& CandidateRecord = Pair.second;
        if (CandidateLight == nullptr || CandidateLight == &RequestLight || !HasAllocatedShadowData(CandidateRecord))
        {
            continue;
        }

        if (IsEvictionProtected(*CandidateLight, CandidateRecord.LightType))
        {
            continue;
        }

        const double CandidateScore = ComputeFinalScore(
            *CandidateLight,
            CandidateRecord.Resolution,
            CandidateRecord.CascadeCount,
            CandidateRecord.LightType);

        if (CandidateScore >= RequestScore)
        {
            continue;
        }

        Candidates.push_back({ CandidateLight, &CandidateRecord, CandidateScore });
    }

    if (Candidates.empty())
    {
        return false;
    }

    std::sort(Candidates.begin(), Candidates.end(), [](const FShadowEvictionCandidate& A, const FShadowEvictionCandidate& B)
    {
        if (A.Score != B.Score)
        {
            return A.Score < B.Score;
        }

        const bool bAPoint = A.Record && A.Record->LightType == static_cast<uint32>(ELightType::Point);
        const bool bBPoint = B.Record && B.Record->LightType == static_cast<uint32>(ELightType::Point);
        if (bAPoint != bBPoint)
        {
            return bAPoint;
        }

        const uint32 AResolution = A.Record ? A.Record->Resolution : 0u;
        const uint32 BResolution = B.Record ? B.Record->Resolution : 0u;
        return AResolution > BResolution;
    });

    FShadowEvictionCandidate& Victim = Candidates.front();
    if (!Victim.Light || !Victim.Record)
    {
        return false;
    }

    UE_LOG(Render, Info, "Evicting %s shadow for %s to free atlas space for %s. VictimScore=%.3f RequestScore=%.3f",
        GetLightTypeName(Victim.Record->LightType),
        GetLightLabel(*Victim.Light).c_str(),
        GetLightLabel(RequestLight).c_str(),
        Victim.Score,
        RequestScore);
    if (const FShadowMapData* VictimAllocation = GetFirstAllocatedShadowData(*Victim.Record))
    {
        UE_LOG(Render, Info, "Evicted allocation detail for %s: resolution=%u %s",
            GetLightLabel(*Victim.Light).c_str(),
            Victim.Record->Resolution,
            FormatShadowAllocation(*VictimAllocation).c_str());
    }

    FreeRecord(*Victim.Record, AtlasPool);
    Victim.Record->LastFailedAllocationFrame = CurrentFrame;
    Victim.Record->FailedAllocationCount++;
    Victim.Light->ClearShadowData();
    return true;
}

double FLightShadowAllocationRegistry::ComputeFinalScore(const FLightProxy& Light, uint32 Resolution, uint32 CascadeCount, uint32 LightType) const
{
    return BuildEvictionDebugInfo(Light, Resolution, CascadeCount, LightType).FinalScore;
}

bool FLightShadowAllocationRegistry::IsEvictionProtected(const FLightProxy& Light, uint32 LightType) const
{
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        return true;
    }

    return Light.Owner == nullptr || !Light.bCastShadow;
}

void FLightShadowAllocationRegistry::InitializeEmptyRecord(
    FLightShadowRecord& Record,
    uint32              RequestedResolution,
    uint32              AllocatedResolution,
    uint32              CascadeCount,
    uint32              LightType) const
{
    Record = {};
    Record.RequestedResolution = RequestedResolution;
    Record.Resolution = AllocatedResolution;
    Record.CascadeCount = CascadeCount;
    Record.LightType = LightType;
    Record.CascadeShadowMapData.CascadeCount = LightType == static_cast<uint32>(ELightType::Directional) ? CascadeCount : 0;
}

FLightShadowAllocationRegistry::FShadowEvictionDebugInfo FLightShadowAllocationRegistry::BuildEvictionDebugInfo(
    const FLightProxy& Light,
    uint32             Resolution,
    uint32             CascadeCount,
    uint32             LightType)
{
    FShadowEvictionDebugInfo Info = {};
    Info.Resolution = Resolution;
    Info.CascadeCount = CascadeCount;
    Info.LightType = LightType;
    Info.TypeWeight = GetLightTypePriorityWeight(LightType);
    Info.IntensityWeight = std::clamp(static_cast<double>(Light.LightProxyInfo.Intensity) / 10.0, 0.0, 10.0) * 25.0;
    Info.CasterWeight = std::clamp(static_cast<double>(Light.VisibleShadowCasters.size()), 0.0, 64.0) * 3.0;
    Info.RadiusWeight = std::clamp(static_cast<double>(Light.LightProxyInfo.AttenuationRadius) / 200.0, 0.0, 25.0);
    Info.DirectionalBonus = (LightType == static_cast<uint32>(ELightType::Directional)) ? 500.0 : 0.0;
    Info.Priority = Info.TypeWeight + Info.IntensityWeight + Info.CasterWeight + Info.RadiusWeight + Info.DirectionalBonus;
    Info.FaceCount = static_cast<double>(GetLightFaceCount(LightType, CascadeCount));
    Info.Cost = std::max(1.0, Info.FaceCount * static_cast<double>(Resolution) * static_cast<double>(Resolution));
    Info.FinalScore = Info.Priority / Info.Cost;
    Info.bEvictionProtected = (LightType == static_cast<uint32>(ELightType::Directional)) || Light.Owner == nullptr || !Light.bCastShadow;
    return Info;
}
