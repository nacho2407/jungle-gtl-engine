#pragma once

#include "Render/Scene/Proxies/Light/LightProxy.h"

class UDirectionalLightComponent;

// FDirectionalLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FDirectionalLightSceneProxy : public FLightProxy
{
public:
    FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void VisualizeLightsInEditor(FScene& Scene, float DebugScale = 1.0f) const override;
    FCascadeShadowMapData*       GetCascadeShadowMapData() override { return &CascadeShadowMapData; }
    const FCascadeShadowMapData* GetCascadeShadowMapData() const override { return &CascadeShadowMapData; }
    int32                        GetCascadeCountSetting() const override { return CascadeCount; }
    float                        GetDynamicShadowDistanceSetting() const override { return DynamicShadowDistance; }
    float                        GetCascadeDistributionSetting() const override { return CascadeDistribution; }

private:
    FCascadeShadowMapData CascadeShadowMapData = {};
    int32                 CascadeCount = 1;
    float                 DynamicShadowDistance = 2000.0f;
    float                 CascadeDistribution = 1.0f;
};
