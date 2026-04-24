// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Render/Scene/Proxies/Light/LightProxy.h"

class UAmbientLightComponent;

// FAmbientLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FAmbientLightSceneProxy : public FLightProxy
{
public:
    FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
    ~FAmbientLightSceneProxy() override = default;

    void UpdateLightConstants() override;
    // 환경광은 공간 속성이 없으므로 Transform 갱신이 필요 없다.
    void UpdateTransform() override {}
};

