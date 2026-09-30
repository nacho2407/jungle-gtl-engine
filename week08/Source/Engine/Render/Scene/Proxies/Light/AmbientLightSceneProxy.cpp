// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/AmbientLightSceneProxy.h"
#include "Component/AmbientLightComponent.h"

FAmbientLightSceneProxy::FAmbientLightSceneProxy(UAmbientLightComponent* InComponent)
    : FLightProxy(InComponent)
{
}

void FAmbientLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightProxy::UpdateLightConstants();

    LightProxyInfo.LightType = static_cast<uint32>(ELightType::Ambient);
}

