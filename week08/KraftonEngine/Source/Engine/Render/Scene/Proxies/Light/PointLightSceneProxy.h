#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UPointLightComponent;

/*
    점광원 컴포넌트를 렌더러용 데이터로 변환하는 프록시입니다.
    위치, 감쇠 반경, 색상 정보를 갱신하고 에디터 구체 시각화를 지원합니다.
*/
class FPointLightSceneProxy : public FLightSceneProxy
{
public:
    FPointLightSceneProxy(UPointLightComponent* InComponent);
    ~FPointLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};
