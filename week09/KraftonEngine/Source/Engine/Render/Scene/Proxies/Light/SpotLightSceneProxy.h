#pragma once

#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

class USpotLightComponent;

/*
    스포트라이트 컴포넌트를 렌더러용 데이터로 변환하는 프록시입니다.
    점광원 정보에 더해 콘 각도와 방향 정보를 함께 관리합니다.
*/
class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
    FSpotLightSceneProxy(USpotLightComponent* InComponent);
    ~FSpotLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void UpdateTransform() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};
