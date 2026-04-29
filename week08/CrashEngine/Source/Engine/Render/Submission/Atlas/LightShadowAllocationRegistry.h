#pragma once

#include "Render/Submission/Atlas/ShadowAtlasPage.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

class FLightProxy;

// 실제 역할은 light -> shadow atlas allocation cache를 관리하는 registry입니다.
class FLightShadowAllocationRegistry
{
public:
    struct FShadowEvictionDebugInfo
    {
        double TypeWeight = 0.0;
        double IntensityWeight = 0.0;
        double CasterWeight = 0.0;
        double RadiusWeight = 0.0;
        double DirectionalBonus = 0.0;
        double Priority = 0.0;
        double FaceCount = 0.0;
        double Cost = 0.0;
        double FinalScore = 0.0;
        bool bEvictionProtected = false;
        uint32 Resolution = 0;
        uint32 CascadeCount = 0;
        uint32 LightType = 0;
    };

    void BeginFrame();
    void EndFrame(FShadowAtlasPool& AtlasPool);
    void Release(FShadowAtlasPool& AtlasPool);
    void RemoveLight(FLightProxy* Light, FShadowAtlasPool& AtlasPool);
    bool UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    FShadowAtlasBudgetStats GetBudgetStats(const FShadowAtlasPool& AtlasPool) const;
    static FShadowEvictionDebugInfo BuildEvictionDebugInfo(const FLightProxy& Light, uint32 Resolution, uint32 CascadeCount, uint32 LightType);

private:
    struct FShadowEvictionCandidate
    {
        FLightProxy* Light = nullptr;
        FLightShadowRecord* Record = nullptr;
        double Score = 0.0;
    };

    void FreeRecord(FLightShadowRecord& Record, FShadowAtlasPool& AtlasPool);
    void SyncLightShadowMatrices(FLightShadowRecord& Record, const FLightProxy& Light) const;
    bool AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool TryAllocateRecordByType(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool, uint32 LightType);
    bool TryAllocateRecord(
        FLightShadowRecord& OutRecord,
        FLightProxy&        Light,
        ID3D11Device*       Device,
        FShadowAtlasPool&   AtlasPool,
        uint32              RequestedResolution,
        uint32              CascadeCount,
        uint32              LightType);
    bool TryEvictForAllocation(
        FLightProxy&      RequestLight,
        FShadowAtlasPool& AtlasPool,
        uint32            RequestedResolution,
        uint32            CascadeCount,
        uint32            LightType);
    double ComputeFinalScore(const FLightProxy& Light, uint32 Resolution, uint32 CascadeCount, uint32 LightType) const;
    bool IsEvictionProtected(const FLightProxy& Light, uint32 LightType) const;
    void InitializeEmptyRecord(
        FLightShadowRecord& Record,
        uint32              RequestedResolution,
        uint32              AllocatedResolution,
        uint32              CascadeCount,
        uint32              LightType) const;

private:
    TMap<FLightProxy*, FLightShadowRecord> Records;
    uint64 CurrentFrame = 0;
    uint32 FailedAllocationCount = 0;
    uint32 EvictedShadowCount = 0;
};
