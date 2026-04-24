// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/DirectionalLightSceneProxy.h"

#include "Component/DirectionalLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
    : FLightProxy(InComponent)
{
}

void FDirectionalLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FLightProxy::UpdateLightConstants();
    LightProxyInfo.LightType = static_cast<uint32>(ELightType::Directional);
}

void FDirectionalLightSceneProxy::VisualizeLightsInEditor(FScene& Scene) const
{
    if (!Owner)
    {
        return;
    }

    const FVector   Origin      = Owner->GetWorldLocation();
    const FVector   Direction   = Owner->GetForwardVector();
    constexpr float ArrowLength = 2.0f;
    const FColor    Color(135, 206, 235);

    RenderDebugArrow(Scene, Origin, Direction, ArrowLength, Color);
}

