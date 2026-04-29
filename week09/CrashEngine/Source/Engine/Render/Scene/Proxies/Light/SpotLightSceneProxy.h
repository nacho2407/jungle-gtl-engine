#pragma once

#include "Render/Scene/Proxies/Light/LightProxy.h"

class USpotLightComponent;

// FSpotLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FSpotLightSceneProxy : public FLightProxy
{
public:
    FSpotLightSceneProxy(USpotLightComponent* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void                   UpdateLightConstants() override;
    void                   UpdateTransform() override;
    void                   VisualizeLightsInEditor(FScene& Scene, float DebugScale = 1.0f) const override;
    FShadowMapData*        GetSpotShadowMapData() override { return &SpotShadowMapData; }
    const FShadowMapData*  GetSpotShadowMapData() const override { return &SpotShadowMapData; }
    FShadowViewData*       GetSpotShadowView() override { return &SpotShadowView; }
    const FShadowViewData* GetSpotShadowView() const override { return &SpotShadowView; }

private:
    FShadowMapData  SpotShadowMapData = {};
    FShadowViewData SpotShadowView    = {};
};
