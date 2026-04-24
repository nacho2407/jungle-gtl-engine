// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UDirectionalLightComponent;

// FDirectionalLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
    FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);
    ~FDirectionalLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    void VisualizeLightsInEditor(FScene& Scene) const override;
};
