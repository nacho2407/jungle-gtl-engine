#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UAmbientLightComponent;

/*
    환경광 컴포넌트를 렌더러용 데이터로 변환하는 프록시입니다.
    공간 위치 없이 장면 전체에 적용되는 Ambient Light 상수만 갱신합니다.
*/
class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
    FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
    ~FAmbientLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    // 환경광은 공간 속성이 없으므로 Transform 갱신이 필요 없다.
    void UpdateTransform() override {}
};
