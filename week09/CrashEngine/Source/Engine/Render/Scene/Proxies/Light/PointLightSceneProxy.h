#pragma once

#include "Render/Scene/Proxies/Light/LightProxy.h"

class UPointLightComponent;

// FPointLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FPointLightSceneProxy : public FLightProxy
{
public:
    FPointLightSceneProxy(UPointLightComponent* InComponent);
    ~FPointLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
    FCubeShadowMapData*       GetCubeShadowMapData() override { return &CubeShadowMapData; }
    const FCubeShadowMapData* GetCubeShadowMapData() const override { return &CubeShadowMapData; }
    FMatrix*                  GetPointShadowViewProjMatrices() override { return ShadowViewProjMatrices; }
    const FMatrix*            GetPointShadowViewProjMatrices() const override { return ShadowViewProjMatrices; }

private:
    FCubeShadowMapData CubeShadowMapData = {};
    FMatrix            ShadowViewProjMatrices[ShadowAtlas::MaxPointFaces] = {};
};
