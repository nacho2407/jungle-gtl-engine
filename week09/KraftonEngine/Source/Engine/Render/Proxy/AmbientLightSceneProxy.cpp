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

    LightConstants = {};
    LightConstants.Intensity  = Owner->GetIntensity();
    LightConstants.LightColor = Owner->GetLightColor();
    LightConstants.LightType  = static_cast<uint32>(ELightType::Ambient);
    // Position, Direction, AttenuationRadius, ConeAngle — 환경광에서 미사용 (0 유지)
}
