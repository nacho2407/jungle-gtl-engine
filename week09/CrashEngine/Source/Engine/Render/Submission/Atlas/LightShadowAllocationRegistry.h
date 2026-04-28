#pragma once

#include "Render/Submission/Atlas/ShadowAtlasPage.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

class FLightProxy;

// 실제 역할은 light -> shadow atlas allocation cache를 관리하는 registry입니다.
class FLightShadowAllocationRegistry
{
public:
    void Release(FShadowAtlasPool& AtlasPool);
    void RemoveLight(FLightProxy* Light, FShadowAtlasPool& AtlasPool);
    bool UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);

private:
    void FreeRecord(FLightShadowRecord& Record, FShadowAtlasPool& AtlasPool);
    void SyncLightShadowMatrices(FLightShadowRecord& Record, const FLightProxy& Light) const;
    bool AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);
    bool AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasPool& AtlasPool);

private:
    TMap<FLightProxy*, FLightShadowRecord> Records;
};
