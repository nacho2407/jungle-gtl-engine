#pragma once

#include "Render/Submission/Atlas/ShadowAtlasPage.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

class FLightProxy;

// 실제 역할은 light -> shadow atlas allocation cache를 관리하는 registry입니다.
class FLightShadowAllocationRegistry
{
public:
    void BeginFrame();
    void EndFrame(FShadowAtlasPool& AtlasPool);
    void Release(FShadowAtlasPool& AtlasPool);
    void RemoveLight(FLightProxy* Light, FShadowAtlasPool& AtlasPool);
    bool UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    FShadowAtlasBudgetStats GetBudgetStats(const FShadowAtlasPool& AtlasPool) const;

private:
    void FreeRecord(FLightShadowRecord& Record, FShadowAtlasPool& AtlasPool);
    void SyncLightShadowMatrices(FLightShadowRecord& Record, const FLightProxy& Light) const;
    bool AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool TryAllocateRecord(
        FLightShadowRecord& OutRecord,
        FLightProxy&        Light,
        ID3D11Device*       Device,
        FShadowAtlasPool&   AtlasPool,
        uint32              RequestedResolution,
        uint32              CascadeCount,
        uint32              LightType);
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
