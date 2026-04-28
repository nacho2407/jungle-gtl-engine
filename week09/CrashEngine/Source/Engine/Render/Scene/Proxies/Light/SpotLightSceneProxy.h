#pragma once

#include "Render/Scene/Proxies/Light/LightProxy.h"

class USpotLightComponent;

// FSpotLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FSpotLightSceneProxy : public FLightProxy
{
public:
    FSpotLightSceneProxy(USpotLightComponent* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
    FShadowMapData*       GetSpotShadowMapData() override { return &SpotShadowMapData; }
    const FShadowMapData* GetSpotShadowMapData() const override { return &SpotShadowMapData; }

private:
    FShadowMapData SpotShadowMapData = {};
};
