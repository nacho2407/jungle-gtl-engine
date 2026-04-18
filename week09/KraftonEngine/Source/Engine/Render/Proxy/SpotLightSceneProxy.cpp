#include "SpotLightSceneProxy.h"
#include "Component/SpotLightComponent.h"

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
    : FPointLightSceneProxy(InComponent)
{
}

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FPointLightSceneProxy::UpdateLightConstants();

    USpotLightComponent* SpotLight = static_cast<USpotLightComponent*>(Owner);
    LightConstants.InnerConeAngle = SpotLight->GetInnerConeAngle();
    LightConstants.OuterConeAngle = SpotLight->GetOuterConeAngle();
    LightConstants.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateTransform()
{
    FLightSceneProxy::UpdateTransform(); // Position + Direction (PointLight의 Position only를 건너뜀)
}