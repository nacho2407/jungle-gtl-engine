#pragma once

#include "Render/Submission/Atlas/ShadowAtlasPage.h"

class FLightProxy;

// Light별 atlas allocation record를 보관하고 재사용 정책을 담당합니다.
struct FLightShadowRecord
{
    uint32 Resolution = 0;
    uint32 CascadeCount = 0;
    uint32 LightType = 0;
    FCascadeShadowMapData CascadeShadowMapData = {};
    FShadowMapData        SpotShadowMapData = {};
    FCubeShadowMapData    CubeShadowMapData = {};
};

// 이름은 Registry지만 실제 역할은 light -> atlas allocation record 매핑 테이블에 가깝습니다.
class FShadowAtlasRegistry
{
public:
    void Release(FShadowAtlasManager& AtlasManager);
    void RemoveLight(FLightProxy* Light, FShadowAtlasManager& AtlasManager);
    bool UpdateLightShadow(FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager);

private:
    void FreeRecord(FLightShadowRecord& Record, FShadowAtlasManager& AtlasManager);
    bool AllocateDirectional(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager);
    bool AllocateSpot(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager);
    bool AllocatePoint(FLightShadowRecord& Record, FLightProxy& Light, ID3D11Device* Device, FShadowAtlasManager& AtlasManager);

private:
    TMap<FLightProxy*, FLightShadowRecord> Records;
};
