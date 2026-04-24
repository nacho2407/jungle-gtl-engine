// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

#include "Component/PointLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
    : FLightProxy(InComponent)
{
    LightProxyInfo.LightType      = static_cast<uint32>(ELightType::Point);
    LightProxyInfo.OuterConeAngle = 180.0f;
    LightProxyInfo.InnerConeAngle = 180.0f;
}

void FPointLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FLightProxy::UpdateLightConstants();

    UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(Owner);
    LightProxyInfo.AttenuationRadius = PointLight->GetAttenuationRadius();
    LightProxyInfo.LightType         = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateTransform()
{
    if (!Owner)
    {
        return;
    }

    LightProxyInfo.Position = Owner->GetWorldLocation();
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

