#include "AmbientLightSceneProxy.h"
#include "Component/AmbientLightComponent.h"

FAmbientLightSceneProxy::FAmbientLightSceneProxy(UAmbientLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
}

void FAmbientLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();

    UAmbientLightComponent* AmbientLight = static_cast<UAmbientLightComponent*>(Owner);
    LightConstants.LightType = static_cast<uint32>(ELightType::Ambient);
}