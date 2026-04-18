#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
}

void FDirectionalLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();
    LightConstants.LightType = static_cast<uint32>(ELightType::Directional);
}

