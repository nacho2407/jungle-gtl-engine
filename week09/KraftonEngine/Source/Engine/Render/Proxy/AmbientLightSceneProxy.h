#pragma once

#include "LightSceneProxy.h"

class UAmbientLightComponent;

/**
 * FAmbientLightSceneProxy
 * UE5의 SkyLight와 유사한 단순 환경광 프록시입니다.
 * 위치·방향이 없으며, 색상과 강도만 GPU 상수로 전달합니다.
 * 셰이더에서는 LightType == Ambient인 경우 감쇠·방향 연산을 건너뜁니다.
 */
class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
    FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
    ~FAmbientLightSceneProxy() override = default;

    void UpdateLightConstants() override;

    // 환경광은 공간 속성이 없으므로 UpdateTransform은 불필요
    void UpdateTransform() override {}
};
