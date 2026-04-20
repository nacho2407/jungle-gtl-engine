#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UDirectionalLightComponent;

/*
    방향성 광원을 렌더러용 데이터로 변환하는 프록시입니다.
    광원 방향과 색, 세기를 갱신하고 에디터 디버그 시각화도 담당합니다.
*/
class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
    FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};
