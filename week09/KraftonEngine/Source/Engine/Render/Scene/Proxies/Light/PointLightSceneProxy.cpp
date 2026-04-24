// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

#include "Component/PointLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
    LightConstants.LightType      = static_cast<uint32>(ELightType::Point);
    LightConstants.OuterConeAngle = 180.0f;
    LightConstants.InnerConeAngle = 180.0f;
}

void FPointLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FLightSceneProxy::UpdateLightConstants();

    UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(Owner);
    LightConstants.AttenuationRadius = PointLight->GetAttenuationRadius();
    LightConstants.LightType         = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateTransform()
{
    if (!Owner)
    {
        return;
    }

    LightConstants.Position = Owner->GetWorldLocation();
}

void FPointLightSceneProxy::VisualizeLightsInEditor(FScene& Scene) const
{
    if (!Owner)
    {
        return;
    }

    UPointLightComponent* Component = static_cast<UPointLightComponent*>(Owner);
    const FVector         Center    = Component->GetWorldLocation();
    const float           Radius    = Component->GetAttenuationRadius();
    const FColor          Color(255, 220, 100);

    RenderDebugSphere(Scene, Center, Radius, 32, Color);
}
