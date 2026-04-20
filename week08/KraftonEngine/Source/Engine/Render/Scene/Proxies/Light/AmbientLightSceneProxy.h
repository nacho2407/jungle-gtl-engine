#pragma once

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"

class UAmbientLightComponent;

class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
    FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);
    ~FAmbientLightSceneProxy() override = default;

    void UpdateLightConstants() override;
	 // 환경광은 공간 속성이 없으므로 UpdateTransform은 불필요
	void UpdateTransform() override {}
};